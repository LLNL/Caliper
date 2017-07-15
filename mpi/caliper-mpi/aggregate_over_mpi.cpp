// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "caliper/cali-mpi.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/CompressedSnapshotRecord.h"
#include "caliper/common/Node.h"
#include "caliper/common/NodeBuffer.h"
#include "caliper/common/SnapshotBuffer.h"

#include <set>

using namespace cali;


namespace
{

void recursive_append_path(const CaliperMetadataAccessInterface& db,
                           const Node* node,
                           NodeBuffer& buf,
                           std::set<cali_id_t>& written_nodes)
{
    if (!node || node->id() == CALI_INV_ID)
        return;
    if (written_nodes.count(node->id()) > 0)
        return;

    if (node->attribute() < node->id())
        recursive_append_path(db, db.node(node->attribute()), buf, written_nodes);

    recursive_append_path(db, node->parent(), buf, written_nodes);

    if (written_nodes.count(node->id()) > 0)
        return;

    written_nodes.insert(node->id());
    buf.append(node);
}    

void pack_and_send(int dest, CaliperMetadataAccessInterface& db, Aggregator& aggregator)
{
    NodeBuffer     nodebuf;
    SnapshotBuffer snapbuf;

    std::set<cali_id_t> written_nodes;

    aggregator.flush(db,
                     [&nodebuf,&snapbuf,&written_nodes](CaliperMetadataAccessInterface& db,
                                                        const EntryList& list)
                     {
                         for (const Entry& e : list)
                             if (e.node())
                                 recursive_append_path(db, e.node(), nodebuf, written_nodes);
                             else if (e.is_immediate())
                                 recursive_append_path(db, db.node(e.attribute()), nodebuf, written_nodes);

                         snapbuf.append(CompressedSnapshotRecord(list.size(), list.data()));
                     });

    {
        unsigned nodecount = nodebuf.count();

        MPI_Send(&nodecount,     1,              MPI_UNSIGNED,
                 dest, 1, MPI_COMM_WORLD);
        MPI_Send(nodebuf.data(), nodebuf.size(), MPI_BYTE,
                 dest, 2, MPI_COMM_WORLD);
    }

    {
        unsigned snapcount = snapbuf.count();

        MPI_Send(&snapcount,     1,              MPI_UNSIGNED,
                 dest, 3, MPI_COMM_WORLD);
        MPI_Send(snapbuf.data(), snapbuf.size(), MPI_BYTE,
                 dest, 4, MPI_COMM_WORLD);
    }        
}


size_t receive_and_merge_nodes(int source, CaliperMetadataDB& db, IdMap& idmap)
{
    unsigned count;

    MPI_Recv(&count, 1, MPI_UNSIGNED,
             source, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    MPI_Status status;
    int size;

    MPI_Probe(source, 2, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_BYTE, &size);

    NodeBuffer nodebuf;

    MPI_Recv(nodebuf.import(size, count), size, MPI_BYTE,
             source, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    nodebuf.for_each([&db,&idmap](const NodeBuffer::NodeInfo& info)
                     {
                         db.merge_node(info.node_id, info.attr_id, info.parent_id, info.value, idmap);
                     });

    return nodebuf.size();
}

size_t receive_and_merge_snapshots(int source, 
                                   CaliperMetadataDB& db, const IdMap& idmap, 
                                   SnapshotProcessFn snap_fn)
{
    unsigned count;

    MPI_Recv(&count, 3, MPI_UNSIGNED,
             source, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    MPI_Status status;
    int size;

    MPI_Probe(source, 4, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_BYTE, &size);

    SnapshotBuffer snapbuf;

    MPI_Recv(snapbuf.import(size, count), size, MPI_BYTE,
             source, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    size_t pos = 0;

    for (size_t i = 0; i < snapbuf.count(); ++i) {
        CompressedSnapshotRecordView view(snapbuf.data()+pos, &pos);

        // currently 127 entries is the max for compressed snapshots
        cali_id_t node_ids[128];
        cali_id_t attr_ids[128];
        Variant   values[128];

        view.unpack_nodes(128, node_ids);
        view.unpack_immediate(128, attr_ids, values);

        snap_fn(db, db.merge_snapshot(view.num_nodes(),      node_ids,
                                      view.num_immediates(), attr_ids, values,
                                      idmap));
    }

    return snapbuf.size();
}

size_t receive_and_merge(int source, CaliperMetadataDB& db, SnapshotProcessFn snap_fn)
{
    IdMap  idmap;
    size_t bytes = 0;

    bytes += receive_and_merge_nodes(source, db, idmap);
    bytes += receive_and_merge_snapshots(source, db, idmap, snap_fn);

    return bytes;
}

    
} // namespace [anonymous]

namespace cali
{

void
aggregate_over_mpi(CaliperMetadataDB& metadb, Aggregator& aggr, MPI_Comm comm)
{
    int commsize;
    int rank;

    MPI_Comm_size(comm, &commsize);
    MPI_Comm_rank(comm, &rank);
    
    for (int step = 0, steppow2 = 1; steppow2 < commsize; ++step, steppow2 *= 2) {
        size_t bytes = 0;
        
        // receive and merge
        if (rank % (2*steppow2) == 0 && rank + steppow2 < commsize)
            bytes = ::receive_and_merge(rank + steppow2, metadb, aggr);

        // send up the tree (happens only in one step for each rank, and never for rank 0)
        if (rank % steppow2 == 0 && rank % (2*steppow2) != 0)
            ::pack_and_send(rank - steppow2, metadb, aggr);
    }
}

}

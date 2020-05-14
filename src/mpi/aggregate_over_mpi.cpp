// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

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

void pack_and_send(int dest, CaliperMetadataAccessInterface& db, Aggregator& aggregator, MPI_Comm comm)
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

        MPI_Send(&nodecount, 1, MPI_UNSIGNED,
                 dest, 1, comm);
        // Work with pre-3.0 MPIs that take non-const void* :-/
        MPI_Send(const_cast<unsigned char*>(nodebuf.data()), nodebuf.size(), MPI_BYTE,
                 dest, 2, comm);
    }

    {
        unsigned snapcount = snapbuf.count();

        MPI_Send(&snapcount, 1, MPI_UNSIGNED,
                 dest, 3, comm);
        MPI_Send(const_cast<unsigned char*>(snapbuf.data()), snapbuf.size(), MPI_BYTE,
                 dest, 4, comm);
    }        
}


size_t receive_and_merge_nodes(int source, CaliperMetadataDB& db, IdMap& idmap, MPI_Comm comm)
{
    unsigned count;

    MPI_Recv(&count, 1, MPI_UNSIGNED,
             source, 1, comm, MPI_STATUS_IGNORE);

    MPI_Status status;
    int size;

    MPI_Probe(source, 2, comm, &status);
    MPI_Get_count(&status, MPI_BYTE, &size);

    NodeBuffer nodebuf;

    MPI_Recv(nodebuf.import(size, count), size, MPI_BYTE,
             source, 2, comm, MPI_STATUS_IGNORE);

    nodebuf.for_each([&db,&idmap](const NodeBuffer::NodeInfo& info)
                     {
                         db.merge_node(info.node_id, info.attr_id, info.parent_id, info.value, idmap);
                     });

    return nodebuf.size();
}

size_t receive_and_merge_snapshots(int source, 
                                   CaliperMetadataDB& db, const IdMap& idmap, 
                                   SnapshotProcessFn snap_fn,
                                   MPI_Comm comm)
{
    unsigned count;

    MPI_Recv(&count, 3, MPI_UNSIGNED,
             source, 3, comm, MPI_STATUS_IGNORE);

    MPI_Status status;
    int size;

    MPI_Probe(source, 4, comm, &status);
    MPI_Get_count(&status, MPI_BYTE, &size);

    SnapshotBuffer snapbuf;

    MPI_Recv(snapbuf.import(size, count), size, MPI_BYTE,
             source, 4, comm, MPI_STATUS_IGNORE);

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

size_t receive_and_merge(int source, CaliperMetadataDB& db, SnapshotProcessFn snap_fn, MPI_Comm comm)
{
    IdMap  idmap;
    size_t bytes = 0;

    bytes += receive_and_merge_nodes(source, db, idmap, comm);
    bytes += receive_and_merge_snapshots(source, db, idmap, snap_fn, comm);

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
        // receive and merge
        if (rank % (2*steppow2) == 0 && rank + steppow2 < commsize)
            ::receive_and_merge(rank + steppow2, metadb, aggr, comm);

        // send up the tree (happens only in one step for each rank, and never for rank 0)
        if (rank % steppow2 == 0 && rank % (2*steppow2) != 0)
            ::pack_and_send(rank - steppow2, metadb, aggr, comm);
    }
}

}

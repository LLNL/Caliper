// Copyright (c) 2018, Lawrence Livermore National Security, LLC.  
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

#include "MpiTracing.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

using namespace cali;


struct MpiTracing::MpiTracingImpl
{        
    // --- The attributes
    //
    
    Attribute call_id_attr;

    Attribute msg_src_attr;
    Attribute msg_dst_attr;
    Attribute msg_size_attr;
    Attribute msg_tag_attr;

    Attribute coll_type_attr;
    Attribute coll_root_attr;

    Attribute comm_attr;
    Attribute comm_is_world_attr;
    Attribute comm_list_attr;
    Attribute comm_size_attr;
    
    // --- MPI object mappings
    //

    struct RequestInfo {
        enum {
            Unknown, Send, Recv
        }            op              { Unknown };
        
        bool         is_persistent   { false   };

        int          target;
        int          tag;
        int          count;
        MPI_Datatype type;

        Node*        comm_node;
    };

    static_assert(sizeof(MPI_Comm)    <= sizeof(uint64_t), "MPI_Comm is larger than u64");
    static_assert(sizeof(MPI_Request) <= sizeof(uint64_t), "MPI_Request is larger than u64");

    std::atomic<int>                          comm_id;

    std::unordered_map<uint64_t, cali::Node*> comm_map; ///< Communicator map
    std::mutex                                comm_map_lock;
    
    std::unordered_map<uint64_t, RequestInfo> req_map;
    std::mutex                                req_map_lock;

    
    // --- Other global
    //

    std::atomic<uint64_t>                     call_id;

    
    // --- initialization
    //
    
    void init_attributes(Caliper* c) {
        const struct attr_info_t {
            const char* name; cali_attr_type type; int prop; Attribute* ptr;
        } attr_info_tbl[] = {
            { "mpi.call.id",       CALI_TYPE_UINT,  CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
              &call_id_attr },
            
            { "mpi.msg.src",       CALI_TYPE_INT,   CALI_ATTR_ASVALUE,
              &msg_src_attr    },
            { "mpi.msg.dst",       CALI_TYPE_INT,   CALI_ATTR_ASVALUE,
              &msg_dst_attr    },
            { "mpi.msg.size",      CALI_TYPE_INT,   CALI_ATTR_ASVALUE,
              &msg_size_attr   },
            { "mpi.msg.tag",       CALI_TYPE_INT,   CALI_ATTR_ASVALUE,
              &msg_tag_attr    },

            { "mpi.coll.type",     CALI_TYPE_INT,   CALI_ATTR_DEFAULT,
              &coll_type_attr  },
            { "mpi.coll.root",     CALI_TYPE_INT,   CALI_ATTR_ASVALUE,
              &coll_root_attr  },

            { "mpi.comm",          CALI_TYPE_INT,   CALI_ATTR_DEFAULT,
              &comm_attr       },
            { "mpi.comm.size",     CALI_TYPE_INT,   CALI_ATTR_DEFAULT,
              &comm_size_attr  },
            { "mpi.comm.is_world", CALI_TYPE_BOOL,  CALI_ATTR_DEFAULT,
              &comm_is_world_attr },
            { "mpi.comm.list",     CALI_TYPE_USR,   CALI_ATTR_DEFAULT,
              &comm_list_attr  },
            
            { nullptr, CALI_TYPE_INV, 0 }
        };

        for (const attr_info_t* p = attr_info_tbl; p->name; ++p)
            *(p->ptr) = c->create_attribute(p->name, p->type, p->prop);
    }

    void init_mpi(Caliper* c) {
        req_map.reserve(100);
        comm_map.reserve(100);
        
        make_comm_entry(c, MPI_COMM_WORLD);
        make_comm_entry(c, MPI_COMM_SELF);
    }

    // --- MPI object lookup
    //

    Node* make_comm_entry(Caliper* c, MPI_Comm comm) {
        int id = comm_id++;

        int size = 0;
        PMPI_Comm_size(comm, &size);

        Node* node = c->make_tree_entry(comm_size_attr, Variant(size));

        int cmp;
        PMPI_Comm_compare(comm, MPI_COMM_WORLD, &cmp);

        if (cmp == MPI_IDENT || cmp == MPI_CONGRUENT)
            node = c->make_tree_entry(comm_is_world_attr, Variant(true), node);

        // TODO: handle lists

        return c->make_tree_entry(comm_attr, Variant(id), node);
    }

    Node* lookup_comm(Caliper* c, MPI_Comm comm) {
        uint64_t key = 0;
        key |= reinterpret_cast<uint64_t>(comm);

        std::lock_guard<std::mutex>
            g(comm_map_lock);

        auto it = comm_map.find(key);

        if (it != comm_map.end())
            return it->second;

        Node* node = make_comm_entry(c, comm);
        comm_map[key] = node;

        return node;
    }

    
    // --- point-to-point
    //

    void handle_send(Caliper* c, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm) {
        int size = 0;
        PMPI_Type_size(type, &size);
        size *= count;

        cali_id_t attr[3] = {
            msg_dst_attr.id(), msg_tag_attr.id(), msg_size_attr.id()
        };
        Variant   data[3] = {
            Variant(dest),     Variant(tag),      Variant(size)
        };

        Node* comm_node = lookup_comm(c, comm);
        
        SnapshotRecord rec(1, &comm_node, 3, attr, data);
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &rec);
    }

    void push_recv_event(Caliper* c, int src, int size, int tag, Node* comm_node) {
        cali_id_t attr[3] = {
            msg_src_attr.id(), msg_tag_attr.id(), msg_size_attr.id()
        };
        Variant   data[3] = {
            Variant(src),      Variant(tag),      Variant(size)
        };
        
        SnapshotRecord rec(1, &comm_node, 3, attr, data);
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &rec);        
    }

    void handle_recv(Caliper* c, MPI_Datatype type, MPI_Comm comm, MPI_Status* status) {
        int size  = 0;
        PMPI_Type_size(type, &size);
        int count = 0;
        PMPI_Get_count(status, type, &count);
        
        push_recv_event(c, status->MPI_SOURCE, size*count, status->MPI_TAG, lookup_comm(c, comm));
    }

    void handle_irecv(Caliper* c, int count, MPI_Datatype type, int src, int tag, MPI_Comm comm, MPI_Request* req) {
        RequestInfo info;

        info.op            = RequestInfo::Recv;
        info.is_persistent = false;
        info.target        = src;
        info.tag           = tag;
        info.type          = type;
        info.count         = count;
        info.comm_node     = lookup_comm(c, comm);

        uint64_t key = 0;
        key |= reinterpret_cast<uint64_t>(*req);

        std::lock_guard<std::mutex>
            g(req_map_lock);

        req_map[key] = info;
    }

    void handle_requests(Caliper* c, int nreq, MPI_Request* reqs, const MPI_Status* statuses) {
        for (int i = 0; i < nreq; ++i) {
            uint64_t key = 0;
            key |= reinterpret_cast<uint64_t>(reqs[i]);

            std::lock_guard<std::mutex>
                g(req_map_lock);
            
            auto it = req_map.find(key);

            if (it == req_map.end())
                continue;

            RequestInfo info = it->second;

            if (info.op == RequestInfo::Recv) {
                int size  = 0;
                PMPI_Type_size(info.type, &size);
                int count = 0;
                PMPI_Get_count(statuses+i, info.type, &count);

                push_recv_event(c, statuses[i].MPI_SOURCE, size*count, statuses[i].MPI_TAG, info.comm_node);
            }
            
            if (!info.is_persistent)
                req_map.erase(it);
        }
    }


    // --- collectives
    //

    void push_coll_event(Caliper* c, CollectiveType coll_type, int size, int root, Node* comm_node) {
        cali_id_t attr[2] = { msg_size_attr.id(), coll_root_attr.id() };
        Variant   data[2] = { Variant(size),      Variant(root)       };

        Node* node = c->make_tree_entry(coll_type_attr, Variant(static_cast<int>(coll_type)), comm_node);

        int ne = 0;

        if (coll_type == Coll_12N || coll_type == Coll_N21)
            ne = 2;
        else if (coll_type == Coll_NxN)
            ne = 1;

        SnapshotRecord rec(1, &node, ne, attr, data);
        c->push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, &rec);
    }
    
    // --- constructor
    //
    
    MpiTracingImpl()
        : comm_id(0),
          call_id(0)
    { }
};


MpiTracing::MpiTracing()
    : mP(new MpiTracingImpl)
{ }

MpiTracing::~MpiTracing()
{
    mP.reset();
}

void
MpiTracing::init(Caliper* c)
{
    mP->init_attributes(c);
}

void
MpiTracing::init_mpi(Caliper* c)
{
    mP->init_mpi(c);
}

void
MpiTracing::handle_send(Caliper* c, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm)
{
    mP->handle_send(c, count, type, dest, tag, comm);
}

void
MpiTracing::handle_recv(Caliper* c, int, MPI_Datatype type, int, int, MPI_Comm comm, MPI_Status* status)
{
    mP->handle_recv(c, type, comm, status);
}

void
MpiTracing::handle_irecv(Caliper* c, int count, MPI_Datatype type, int src, int tag, MPI_Comm comm, MPI_Request* req)
{
    mP->handle_irecv(c, count, type, src, tag, comm, req);
}

void
MpiTracing::handle_requests(Caliper* c, int nreq, MPI_Request* reqs, const MPI_Status* statuses)
{
    mP->handle_requests(c, nreq, reqs, statuses);
}

void
MpiTracing::handle_12n(Caliper* c, int count, MPI_Datatype type, int root, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);
    int rank = 0;
    PMPI_Comm_rank(comm, &rank);

    mP->push_coll_event(c, Coll_12N, (rank == root ? count : 0) * size, root, mP->lookup_comm(c, comm));
}

void
MpiTracing::handle_n21(Caliper* c, int count, MPI_Datatype type, int root, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);
    int rank = 0;
    PMPI_Comm_rank(comm, &rank);

    mP->push_coll_event(c, Coll_N21, (rank != root ? count : 0) * size, root, mP->lookup_comm(c, comm));
}

void
MpiTracing::handle_n2n(Caliper* c, int count, MPI_Datatype type, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);

    mP->push_coll_event(c, Coll_NxN, count*size, 0, mP->lookup_comm(c, comm));
}

void
MpiTracing::handle_barrier(Caliper* c, MPI_Comm comm)
{
    mP->push_coll_event(c, Coll_Barrier, 0, 0, mP->lookup_comm(c, comm));
}

void
MpiTracing::push_call_id(Caliper* c)
{
    c->begin(mP->call_id_attr, ++(mP->call_id));
}

void
MpiTracing::pop_call_id(Caliper* c)
{
    c->end(mP->call_id_attr);
}

// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "MpiTracing.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include <atomic>
#include <mutex>
#include <numeric>
#include <unordered_map>

using namespace cali;


struct MpiTracing::MpiTracingImpl
{
    // --- The attributes
    //

    Attribute msg_src_attr;
    Attribute msg_dst_attr;
    Attribute msg_size_attr;
    Attribute msg_tag_attr;
    Attribute send_count_attr;
    Attribute recv_count_attr;

    Attribute coll_type_attr;
    Attribute coll_root_attr;
    Attribute coll_count_attr;

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
        int          size;

        Node*        comm_node;
    };

    std::atomic<int>                             comm_id;

    // We hope that whatever MPI_Comm and MPI_Request is is default-hashable.
    // So far it works ...

    std::unordered_map<MPI_Comm,    cali::Node*> comm_map; ///< Communicator map
    std::mutex                                   comm_map_lock;

    std::unordered_map<MPI_Request, RequestInfo> req_map;
    std::mutex                                   req_map_lock;

    // --- initialization
    //

    void init_attributes(Caliper* c) {
        const struct attr_info_t {
            const char* name; cali_attr_type type; int prop; Attribute* ptr;
        } attr_info_tbl[] = {
            { "mpi.msg.src",       CALI_TYPE_INT,   CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
              &msg_src_attr    },
            { "mpi.msg.dst",       CALI_TYPE_INT,   CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
              &msg_dst_attr    },
            { "mpi.msg.tag",       CALI_TYPE_INT,   CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
              &msg_tag_attr    },

            { "mpi.coll.type",     CALI_TYPE_INT,   CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS,
              &coll_type_attr  },
            { "mpi.coll.root",     CALI_TYPE_INT,   CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
              &coll_root_attr  },

            { "mpi.comm",          CALI_TYPE_INT,   CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS,
              &comm_attr       },
            { "mpi.comm.size",     CALI_TYPE_INT,   CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS,
              &comm_size_attr  },
            { "mpi.comm.is_world", CALI_TYPE_BOOL,  CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS,
              &comm_is_world_attr },
            { "mpi.comm.list",     CALI_TYPE_USR,   CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS,
              &comm_list_attr  },

            { "mpi.msg.size",      CALI_TYPE_INT,   CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
              &msg_size_attr   },
            { "mpi.send.count",    CALI_TYPE_INT,   CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
              &send_count_attr },
            { "mpi.recv.count",    CALI_TYPE_INT,   CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
              &recv_count_attr },
            { "mpi.coll.count",    CALI_TYPE_INT,   CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
              &coll_count_attr },

            { nullptr, CALI_TYPE_INV, 0, nullptr }
        };

        for (const struct attr_info_t* a = attr_info_tbl; a && a->name; a++)
            *(a->ptr) = c->create_attribute(a->name, a->type, a->prop);
    }

    void init_mpi(Caliper* c, Channel* chn) {
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
        else {
            std::vector<int> ranks_in(size, 0);
            std::iota(ranks_in.begin(), ranks_in.end(), 0); // create sequence 0,1, ... n-1

            std::vector<int> ranks_out(size, 0);

            MPI_Group world_grp;
            MPI_Group comm_grp;

            PMPI_Comm_group(MPI_COMM_WORLD, &world_grp);
            PMPI_Comm_group(comm, &comm_grp);

            PMPI_Group_translate_ranks(comm_grp, size, ranks_in.data(), world_grp, ranks_out.data());

            node =
                c->make_tree_entry(comm_list_attr,
                                   Variant(CALI_TYPE_USR, ranks_out.data(), ranks_out.size()*sizeof(int)),
                                   node);
        }

        return c->make_tree_entry(comm_attr, Variant(id), node);
    }

    Node* lookup_comm(Caliper* c, MPI_Comm comm) {
        std::lock_guard<std::mutex>
            g(comm_map_lock);

        auto it = comm_map.find(comm);

        if (it != comm_map.end())
            return it->second;

        Node* node = make_comm_entry(c, comm);
        comm_map[comm] = node;

        return node;
    }


    // --- point-to-point
    //

    void push_send_event(Caliper* c, Channel* channel, int size, int dest, int tag, cali::Node* comm_node) {
        const Entry data[] = {
            { comm_node },
            { msg_dst_attr,    Variant(dest) },
            { msg_tag_attr,    Variant(tag)  },
            { msg_size_attr,   Variant(size) },
            { send_count_attr, Variant(1)    }
        };

        c->push_snapshot(channel, SnapshotView(5, data));
    }

    void handle_send_init(Caliper* c, Channel* chn, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm, MPI_Request* req) {
        RequestInfo info;

        info.op            = RequestInfo::Send;
        info.is_persistent = true;
        info.target        = dest;
        info.tag           = tag;
        info.count         = count;
        info.type          = type;
        info.comm_node     = lookup_comm(c, comm);

        PMPI_Type_size(type, &info.size);
        info.size *= count;

        std::lock_guard<std::mutex>
            g(req_map_lock);

        req_map[*req] = info;
    }

    void push_recv_event(Caliper* c, Channel* channel, int src, int size, int tag, Node* comm_node) {
        const Entry data[] = {
            { comm_node },
            { msg_src_attr,    Variant(src)  },
            { msg_tag_attr,    Variant(tag)  },
            { msg_size_attr,   Variant(size) },
            { recv_count_attr, Variant(1)    }
        };

        c->push_snapshot(channel, SnapshotView(5, data));
    }

    void handle_recv(Caliper* c, Channel* chn, MPI_Datatype type, MPI_Comm comm, MPI_Status* status) {
        int size  = 0;
        PMPI_Type_size(type, &size);
        int count = 0;
        PMPI_Get_count(status, type, &count);

        push_recv_event(c, chn, status->MPI_SOURCE, size*count, status->MPI_TAG, lookup_comm(c, comm));
    }

    void handle_irecv(Caliper* c, Channel* chn, int count, MPI_Datatype type, int src, int tag, MPI_Comm comm, MPI_Request* req) {
        RequestInfo info;

        info.op            = RequestInfo::Recv;
        info.is_persistent = false;
        info.target        = src;
        info.tag           = tag;
        info.type          = type;
        info.count         = count;
        info.comm_node     = lookup_comm(c, comm);

        std::lock_guard<std::mutex>
            g(req_map_lock);

        req_map[*req] = info;
    }

    void handle_recv_init(Caliper* c, Channel* chn, int count, MPI_Datatype type, int src, int tag, MPI_Comm comm, MPI_Request* req) {
        RequestInfo info;

        info.op            = RequestInfo::Recv;
        info.is_persistent = true;
        info.target        = src;
        info.tag           = tag;
        info.type          = type;
        info.count         = count;
        info.comm_node     = lookup_comm(c, comm);
        info.size          = 0;

        std::lock_guard<std::mutex>
            g(req_map_lock);

        req_map[*req] = info;
    }

    void handle_start(Caliper* c, Channel* chn, int nreq, MPI_Request* reqs) {
        for (int i = 0; i < nreq; ++i) {
            std::lock_guard<std::mutex>
                g(req_map_lock);

            auto it = req_map.find(reqs[i]);

            if (it == req_map.end())
                continue;

            RequestInfo info = it->second;

            if (info.op == RequestInfo::Send)
                push_send_event(c, chn, info.size, info.target, info.tag, info.comm_node);
        }
    }

    void handle_completion(Caliper* c, Channel* chn, int nreq, MPI_Request* reqs, MPI_Status* statuses) {
        for (int i = 0; i < nreq; ++i) {
            std::lock_guard<std::mutex>
                g(req_map_lock);

            auto it = req_map.find(reqs[i]);

            if (it == req_map.end())
                continue;

            RequestInfo info = it->second;

            if (info.op == RequestInfo::Recv) {
                int size  = 0;
                PMPI_Type_size(info.type, &size);
                int count = 0;
                PMPI_Get_count(statuses+i, info.type, &count);

                push_recv_event(c, chn, statuses[i].MPI_SOURCE, size*count, statuses[i].MPI_TAG, info.comm_node);
            }

            if (!info.is_persistent)
                req_map.erase(it);
        }
    }

    void request_free(MPI_Request* req) {
        std::lock_guard<std::mutex>
            g(req_map_lock);

        req_map.erase(*req);
    }

    // --- collectives
    //

    void push_coll_event(Caliper* c, Channel* channel, CollectiveType coll_type, int size, int root, Node* comm_node) {
        Node* node = c->make_tree_entry(coll_type_attr, Variant(static_cast<int>(coll_type)), comm_node);

        const Entry data[] = {
            { node },
            { coll_count_attr, Variant(1)    },
            { msg_size_attr,   Variant(size) },
            { coll_root_attr,  Variant(root) }
        };

        int ne = 2;

        if (coll_type == Coll_12N || coll_type == Coll_N21)
            ne = 4;
        else if (coll_type == Coll_NxN)
            ne = 3;

        c->push_snapshot(channel, SnapshotView(ne, data));
    }

    // --- constructor
    //

    MpiTracingImpl()
        : comm_id(0)
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
MpiTracing::init(Caliper* c, Channel* chn)
{
    mP->init_attributes(c);
}

void
MpiTracing::init_mpi(Caliper* c, Channel* chn)
{
    mP->init_mpi(c, chn);
}

void
MpiTracing::handle_send(Caliper* c, Channel* chn, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);
    size *= count;

    mP->push_send_event(c, chn, size, dest, tag, mP->lookup_comm(c, comm));
}

void
MpiTracing::handle_send_init(Caliper* c, Channel* chn, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm, MPI_Request* req)
{
    mP->handle_send_init(c, chn, count, type, dest, tag, comm, req);
}

void
MpiTracing::handle_recv(Caliper* c, Channel* chn, int, MPI_Datatype type, int, int, MPI_Comm comm, MPI_Status* status)
{
    mP->handle_recv(c, chn, type, comm, status);
}

void
MpiTracing::handle_irecv(Caliper* c, Channel* chn, int count, MPI_Datatype type, int src, int tag, MPI_Comm comm, MPI_Request* req)
{
    mP->handle_irecv(c, chn, count, type, src, tag, comm, req);
}

void
MpiTracing::handle_recv_init(Caliper* c, Channel* chn, int count, MPI_Datatype type, int src, int tag, MPI_Comm comm, MPI_Request* req)
{
    mP->handle_recv_init(c, chn, count, type, src, tag, comm, req);
}

void
MpiTracing::handle_start(Caliper* c, Channel* chn, int nreq, MPI_Request* reqs)
{
    mP->handle_start(c, chn, nreq, reqs);
}

void
MpiTracing::handle_completion(Caliper* c, Channel* chn, int nreq, MPI_Request* reqs, MPI_Status* statuses)
{
    mP->handle_completion(c, chn, nreq, reqs, statuses);
}

void
MpiTracing::request_free(Caliper*, Channel*, MPI_Request* req)
{
    mP->request_free(req);
}

void
MpiTracing::handle_12n(Caliper* c, Channel* chn, int count, MPI_Datatype type, int root, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);
    int rank = 0;
    PMPI_Comm_rank(comm, &rank);

    mP->push_coll_event(c, chn, Coll_12N, (rank == root ? count : 0) * size, root, mP->lookup_comm(c, comm));
}

void
MpiTracing::handle_n21(Caliper* c, Channel* chn, int count, MPI_Datatype type, int root, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);
    int rank = 0;
    PMPI_Comm_rank(comm, &rank);

    mP->push_coll_event(c, chn, Coll_N21, (rank != root ? count : 0) * size, root, mP->lookup_comm(c, comm));
}

void
MpiTracing::handle_n2n(Caliper* c, Channel* chn, int count, MPI_Datatype type, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);

    mP->push_coll_event(c, chn, Coll_NxN, count*size, 0, mP->lookup_comm(c, comm));
}

void
MpiTracing::handle_barrier(Caliper* c, Channel* chn, MPI_Comm comm)
{
    mP->push_coll_event(c, chn, Coll_Barrier, 0, 0, mP->lookup_comm(c, comm));
}

void
MpiTracing::handle_init(Caliper* c, Channel* chn)
{
    mP->push_coll_event(c, chn, Coll_Init, 0, 0, mP->lookup_comm(c, MPI_COMM_WORLD));
}

void
MpiTracing::handle_finalize(Caliper* c, Channel* chn)
{
    mP->push_coll_event(c, chn, Coll_Finalize, 0, 0, mP->lookup_comm(c, MPI_COMM_WORLD));
}

// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "MpiPattern.h"

#include "caliper/AsyncEvent.h"
#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include <atomic>
#include <mutex>
#include <numeric>
#include <unordered_map>
#include <sstream>
#include <set>

using namespace cali;

struct MpiPattern::MpiPatternImpl {
    // --- The attributes
    //

    Attribute total_send_count_attr;
    Attribute total_recv_count_attr;
    Attribute total_coll_count_attr;
    Attribute total_dest_ranks_attr;
    Attribute total_src_ranks_attr;
    Attribute total_recv_size_attr;
    Attribute total_send_size_attr;
    Attribute total_coll_size_attr;

    // --- MPI object mappings
    //

    struct RequestInfo {
        enum { Unknown, Send, Recv } op { Unknown };

        bool is_persistent { false };

        int          target;
        int          tag;
        int          count;
        MPI_Datatype type;
        int          size;

        TimedAsyncEvent timer;
    };

    std::unordered_map<MPI_Request, RequestInfo> req_map;
    std::mutex                                   req_map_lock;

    // --- initialization
    //

    void init_attributes(Caliper* c)
    {
        const struct attr_info_t {
            const char*    name;
            cali_attr_type type;
            int            prop;
            Attribute*     ptr;
        } attr_info_tbl[] = { { "mpi.recv.size",
                                CALI_TYPE_INT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
                                &total_recv_size_attr },
                              { "mpi.send.size",
                                CALI_TYPE_INT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
                                &total_send_size_attr },
                              { "mpi.coll.size",
                                CALI_TYPE_INT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
                                &total_coll_size_attr },

                              { "total.send.count",
                                CALI_TYPE_INT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
                                &total_send_count_attr },
                              { "total.recv.count",
                                CALI_TYPE_INT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
                                &total_recv_count_attr },
                              { "total.coll.count",
                                CALI_TYPE_INT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
                                &total_coll_count_attr },
                              { "total.dest.ranks",
                                CALI_TYPE_INT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
                                &total_dest_ranks_attr },
                              { "total.src.ranks",
                                CALI_TYPE_INT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE,
                                &total_src_ranks_attr },

                              { nullptr, CALI_TYPE_INV, 0, nullptr } };

        for (const struct attr_info_t* a = attr_info_tbl; a && a->name; a++)
            *(a->ptr) = c->create_attribute(a->name, a->type, a->prop);
    }

    void init_mpi(Caliper* c, Channel* chn)
    {
        req_map.reserve(100);
    }

    // --- point-to-point
    //

    int total_send_count = 0;
    int total_recv_count = 0;
    int total_coll_count = 0;
    int total_send_size  = 0;
    int total_recv_size  = 0;
    int total_coll_size  = 0;

    std::set<int> unique_src_ranks;
    std::set<int> unique_dest_ranks;

    void push_send_event(Caliper* c, ChannelBody*, int size, int dest, int tag)
    {
        total_send_count++;
        total_send_size += size;

        unique_dest_ranks.insert(dest);
    }

    void handle_send_init(
        Caliper*     c,
        ChannelBody* chB,
        int          count,
        MPI_Datatype type,
        int          dest,
        int          tag,
        MPI_Comm     comm,
        MPI_Request* req
    )
    {
        RequestInfo info;

        info.op            = RequestInfo::Send;
        info.is_persistent = true;
        info.target        = dest;
        info.tag           = tag;
        info.count         = count;
        info.type          = type;

        PMPI_Type_size(type, &info.size);
        info.size *= count;

        std::lock_guard<std::mutex> g(req_map_lock);

        req_map[*req] = info;
    }

    void push_recv_event(Caliper* c, ChannelBody*, int src, int size, int tag)
    {
        total_recv_count++;
        total_recv_size += size;

        unique_src_ranks.insert(src);
    }

    void handle_recv(Caliper* c, ChannelBody* chB, MPI_Datatype type, MPI_Comm comm, MPI_Status* status)
    {
        int size = 0;
        PMPI_Type_size(type, &size);
        int count = 0;
        PMPI_Get_count(status, type, &count);

        push_recv_event(c, chB, status->MPI_SOURCE, size * count, status->MPI_TAG);
    }

    void handle_irecv(
        Caliper*     c,
        ChannelBody* chB,
        int          count,
        MPI_Datatype type,
        int          src,
        int          tag,
        MPI_Comm     comm,
        MPI_Request* req
    )
    {
        RequestInfo info;

        info.op            = RequestInfo::Recv;
        info.is_persistent = false;
        info.target        = src;
        info.tag           = tag;
        info.type          = type;
        info.count         = count;
        info.timer         = TimedAsyncEvent::begin("irecv.req_wait_gap");

        std::lock_guard<std::mutex> g(req_map_lock);

        req_map[*req] = info;
    }

    void handle_recv_init(
        Caliper*     c,
        ChannelBody* chB,
        int          count,
        MPI_Datatype type,
        int          src,
        int          tag,
        MPI_Comm     comm,
        MPI_Request* req
    )
    {
        RequestInfo info;

        info.op            = RequestInfo::Recv;
        info.is_persistent = true;
        info.target        = src;
        info.tag           = tag;
        info.type          = type;
        info.count         = count;
        info.size          = 0;

        std::lock_guard<std::mutex> g(req_map_lock);

        req_map[*req] = info;
    }

    void handle_start(Caliper* c, ChannelBody* chB, int nreq, MPI_Request* reqs)
    {
        for (int i = 0; i < nreq; ++i) {
            std::lock_guard<std::mutex> g(req_map_lock);

            auto it = req_map.find(reqs[i]);

            if (it == req_map.end())
                continue;

            RequestInfo info = it->second;

            if (info.op == RequestInfo::Send)
                push_send_event(c, chB, info.size, info.target, info.tag);
        }
    }

    void handle_pre_completion(Caliper* c, ChannelBody* chB, int nreq, MPI_Request* reqs)
    {
        for (int i = 0; i < nreq; ++i) {
            std::lock_guard<std::mutex> g(req_map_lock);

            auto it = req_map.find(reqs[i]);
            if (it == req_map.end())
                continue;

            if (it->second.op == RequestInfo::Recv)
                it->second.timer.end();
        }
    }

    void handle_completion(Caliper* c, ChannelBody* chB, int nreq, MPI_Request* reqs, MPI_Status* statuses)
    {
        for (int i = 0; i < nreq; ++i) {
            std::lock_guard<std::mutex> g(req_map_lock);

            auto it = req_map.find(reqs[i]);

            if (it == req_map.end())
                continue;

            RequestInfo info = it->second;

            if (info.op == RequestInfo::Recv) {
                int size = 0;
                PMPI_Type_size(info.type, &size);
                int count = 0;
                PMPI_Get_count(statuses + i, info.type, &count);

                push_recv_event(c, chB, statuses[i].MPI_SOURCE, size * count, statuses[i].MPI_TAG);
            }

            if (!info.is_persistent)
                req_map.erase(it);
        }
    }

    void request_free(MPI_Request* req)
    {
        std::lock_guard<std::mutex> g(req_map_lock);

        req_map.erase(*req);
    }

    // --- collectives
    //

    void push_coll_event(Caliper* c, ChannelBody* chB, CollectiveType coll_type, int size, int root)
    {
        ++total_coll_count;
        total_coll_size += size;
    }

    void handle_comm_begin(Caliper* c, ChannelBody* chn)
    {
        total_send_count = 0;
        total_recv_count = 0;
        total_coll_count = 0;
        total_recv_size  = 0;
        total_send_size  = 0;
        total_coll_size  = 0;

        unique_dest_ranks.clear();
        unique_src_ranks.clear();
    }

    void handle_comm_end(Caliper* c, ChannelBody* chn)
    {
        const Entry data[] = { { total_send_count_attr, Variant(total_send_count) },
                               { total_recv_count_attr, Variant(total_recv_count) },
                               { total_coll_count_attr, Variant(total_coll_count) },

                               { total_dest_ranks_attr, Variant(unique_dest_ranks.size()) },
                               { total_src_ranks_attr, Variant(unique_src_ranks.size()) },

                               { total_recv_size_attr, Variant(total_recv_size) },
                               { total_send_size_attr, Variant(total_send_size) },
                               { total_coll_size_attr, Variant(total_coll_size) } };

        c->push_snapshot(chn, SnapshotView(8, data));
    }

    // --- constructor
    //

    MpiPatternImpl() {}
};

MpiPattern::MpiPattern() : mP(new MpiPatternImpl)
{}

MpiPattern::~MpiPattern()
{
    mP.reset();
}

void MpiPattern::init(Caliper* c, Channel* chn)
{
    mP->init_attributes(c);
}

void MpiPattern::init_mpi(Caliper* c, Channel* chn)
{
    mP->init_mpi(c, chn);
}

void MpiPattern::handle_send(Caliper* c, ChannelBody* chB, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);
    size *= count;

    mP->push_send_event(c, chB, size, dest, tag);
}

void MpiPattern::handle_send_init(
    Caliper*     c,
    ChannelBody*     chB,
    int          count,
    MPI_Datatype type,
    int          dest,
    int          tag,
    MPI_Comm     comm,
    MPI_Request* req
)
{
    mP->handle_send_init(c, chB, count, type, dest, tag, comm, req);
}

void MpiPattern::handle_recv(
    Caliper* c,
    ChannelBody* chB,
    int,
    MPI_Datatype type,
    int,
    int,
    MPI_Comm    comm,
    MPI_Status* status
)
{
    mP->handle_recv(c, chB, type, comm, status);
}

void MpiPattern::handle_irecv(
    Caliper*     c,
    ChannelBody*     chB,
    int          count,
    MPI_Datatype type,
    int          src,
    int          tag,
    MPI_Comm     comm,
    MPI_Request* req
)
{
    mP->handle_irecv(c, chB, count, type, src, tag, comm, req);
}

void MpiPattern::handle_recv_init(
    Caliper*     c,
    ChannelBody*     chB,
    int          count,
    MPI_Datatype type,
    int          src,
    int          tag,
    MPI_Comm     comm,
    MPI_Request* req
)
{
    mP->handle_recv_init(c, chB, count, type, src, tag, comm, req);
}

void MpiPattern::handle_start(Caliper* c, ChannelBody* chB, int nreq, MPI_Request* reqs)
{
    mP->handle_start(c, chB, nreq, reqs);
}

void MpiPattern::handle_pre_completion(Caliper* c, ChannelBody* chB, int nreq, MPI_Request* reqs)
{
    mP->handle_pre_completion(c, chB, nreq, reqs);
}

void MpiPattern::handle_completion(Caliper* c, ChannelBody* chB, int nreq, MPI_Request* reqs, MPI_Status* statuses)
{
    mP->handle_completion(c, chB, nreq, reqs, statuses);
}

void MpiPattern::request_free(Caliper*, ChannelBody*, MPI_Request* req)
{
    mP->request_free(req);
}

void MpiPattern::handle_12n(Caliper* c, ChannelBody* chB, int count, MPI_Datatype type, int root, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);
    int rank = 0;
    PMPI_Comm_rank(comm, &rank);

    mP->push_coll_event(c, chB, Coll_12N, (rank == root ? count : 0) * size, root);
}

void MpiPattern::handle_n21(Caliper* c, ChannelBody* chB, int count, MPI_Datatype type, int root, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);
    int rank = 0;
    PMPI_Comm_rank(comm, &rank);

    mP->push_coll_event(c, chB, Coll_N21, (rank != root ? count : 0) * size, root);
}

void MpiPattern::handle_n2n(Caliper* c, ChannelBody* chB, int count, MPI_Datatype type, MPI_Comm comm)
{
    int size = 0;
    PMPI_Type_size(type, &size);

    mP->push_coll_event(c, chB, Coll_NxN, count * size, 0);
}

void MpiPattern::handle_barrier(Caliper* c, ChannelBody* chB, MPI_Comm comm)
{
    mP->push_coll_event(c, chB, Coll_Barrier, 0, 0);
}

void MpiPattern::handle_init(Caliper* c, ChannelBody* chB)
{
    mP->push_coll_event(c, chB, Coll_Init, 0, 0);
}

void MpiPattern::handle_finalize(Caliper* c, ChannelBody* chB)
{
    mP->push_coll_event(c, chB, Coll_Finalize, 0, 0);
}

void MpiPattern::handle_comm_begin(Caliper* c, ChannelBody* chB)
{
    mP->handle_comm_begin(c, chB);
}

void MpiPattern::handle_comm_end(Caliper* c, ChannelBody* chB)
{
    mP->handle_comm_end(c, chB);
}

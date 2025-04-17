// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file MsgPattern.h
/// \brief MPI communication pattern analysis

#include <mpi.h>

#include <memory>

#pragma once

namespace cali
{

class Caliper;
class Channel;

struct ChannelBody;

class MpiPattern
{
    struct MpiPatternImpl;
    std::unique_ptr<MpiPatternImpl> mP;

public:

    enum CollectiveType { Unknown, Coll_Barrier, Coll_NxN, Coll_12N, Coll_N21, Coll_Init, Coll_Finalize };

    MpiPattern();

    ~MpiPattern();

    void init(Caliper* c, Channel* chn);
    void init_mpi(Caliper* c, Channel* chn);

    // --- point-to-point

    void handle_send(Caliper* c, ChannelBody* chB, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm);
    void handle_send_init(
        Caliper*     c,
        ChannelBody* chB,
        int          count,
        MPI_Datatype type,
        int          dest,
        int          tag,
        MPI_Comm     comm,
        MPI_Request* req
    );

    void handle_recv(
        Caliper*     c,
        ChannelBody* chB,
        int          count,
        MPI_Datatype type,
        int          src,
        int          tag,
        MPI_Comm     comm,
        MPI_Status*  status
    );
    void handle_irecv(
        Caliper*     c,
        ChannelBody* chB,
        int          count,
        MPI_Datatype type,
        int          src,
        int          tag,
        MPI_Comm     comm,
        MPI_Request* req
    );
    void handle_recv_init(
        Caliper*     c,
        ChannelBody* chB,
        int          count,
        MPI_Datatype type,
        int          src,
        int          tag,
        MPI_Comm     comm,
        MPI_Request* req
    );

    void handle_start(Caliper* c, ChannelBody* chB, int nreq, MPI_Request* reqs);
    void handle_pre_completion(Caliper *c, ChannelBody* chB, int nreq, MPI_Request* reqs);
    void handle_completion(Caliper* c, ChannelBody* chB, int nreq, MPI_Request* reqs, MPI_Status* statuses);

    void handle_comm_begin(Caliper* c, ChannelBody* chB); //region_begin
    void handle_comm_end(Caliper* c, ChannelBody* chB);   //region_end

    void request_free(Caliper* c, ChannelBody* chB, MPI_Request* req);

    // --- collectives

    void handle_12n(Caliper* c, ChannelBody* chB, int count, MPI_Datatype type, int root, MPI_Comm comm);
    void handle_n21(Caliper* c, ChannelBody* chB, int count, MPI_Datatype type, int root, MPI_Comm comm);
    void handle_n2n(Caliper* c, ChannelBody* chB, int count, MPI_Datatype type, MPI_Comm comm);
    void handle_barrier(Caliper* c, ChannelBody* chB, MPI_Comm comm);
    void handle_init(Caliper* c, ChannelBody* chB);
    void handle_finalize(Caliper* c, ChannelBody* chB);
};

} // namespace cali

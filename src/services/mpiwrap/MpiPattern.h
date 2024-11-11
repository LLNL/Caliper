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

    void handle_send(Caliper* c, Channel* chn, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm);
    void handle_send_init(
        Caliper*     c,
        Channel*     chn,
        int          count,
        MPI_Datatype type,
        int          dest,
        int          tag,
        MPI_Comm     comm,
        MPI_Request* req
    );

    void handle_recv(
        Caliper*     c,
        Channel*     chn,
        int          count,
        MPI_Datatype type,
        int          src,
        int          tag,
        MPI_Comm     comm,
        MPI_Status*  status
    );
    void handle_irecv(
        Caliper*     c,
        Channel*     chn,
        int          count,
        MPI_Datatype type,
        int          src,
        int          tag,
        MPI_Comm     comm,
        MPI_Request* req
    );
    void handle_recv_init(
        Caliper*     c,
        Channel*     chn,
        int          count,
        MPI_Datatype type,
        int          src,
        int          tag,
        MPI_Comm     comm,
        MPI_Request* req
    );

    void handle_start(Caliper* c, Channel* chn, int nreq, MPI_Request* reqs);
    void handle_pre_completion(Caliper *c, Channel* chn, int nreq, MPI_Request* reqs);
    void handle_completion(Caliper* c, Channel* chn, int nreq, MPI_Request* reqs, MPI_Status* statuses);

    void handle_comm_begin(Caliper* c, Channel* chn); //region_begin
    void handle_comm_end(Caliper* c, Channel* chn);   //region_end

    void request_free(Caliper* c, Channel* chn, MPI_Request* req);

    // --- collectives

    void handle_12n(Caliper* c, Channel* chn, int count, MPI_Datatype type, int root, MPI_Comm comm);
    void handle_n21(Caliper* c, Channel* chn, int count, MPI_Datatype type, int root, MPI_Comm comm);
    void handle_n2n(Caliper* c, Channel* chn, int count, MPI_Datatype type, MPI_Comm comm);
    void handle_barrier(Caliper* c, Channel* chn, MPI_Comm comm);
    void handle_init(Caliper* c, Channel* chn);
    void handle_finalize(Caliper* c, Channel* chn);
};

} // namespace cali

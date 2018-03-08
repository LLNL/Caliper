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

/// \file MsgTrace.h
/// \brief MPI communication tracer

#include <mpi.h>

#include <memory>

#pragma once

namespace cali
{

class Caliper;

class MpiTracing
{
    struct MpiTracingImpl;
    std::unique_ptr<MpiTracingImpl> mP;

public:

    enum CollectiveType {
        Unknown, Coll_Barrier, Coll_NxN, Coll_12N, Coll_N21, Coll_Init, Coll_Finalize
    };

    MpiTracing();

    ~MpiTracing();

    void init(Caliper* c);
    void init_mpi(Caliper* c);

    void push_call_id(Caliper* c);
    void pop_call_id(Caliper* c);

    // --- point-to-point

    void handle_send(Caliper* c, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm);
    void handle_send_init(Caliper* c, int count, MPI_Datatype type, int dest, int tag, MPI_Comm comm, MPI_Request* req);

    void handle_recv(Caliper* c, int count, MPI_Datatype type, int src, int tag, MPI_Comm comm, MPI_Status* status);
    void handle_irecv(Caliper* c, int count, MPI_Datatype type, int src, int tag, MPI_Comm comm, MPI_Request* req);
    void handle_recv_init(Caliper* c, int count, MPI_Datatype type, int src, int tag, MPI_Comm comm, MPI_Request* req);

    void handle_start(Caliper* c, int nreq, MPI_Request* reqs);
    void handle_completion(Caliper* c, int nreq, MPI_Request* reqs, MPI_Status* statuses);

    void request_free(Caliper* c, MPI_Request* req);

    // --- collectives

    void handle_12n(Caliper* c, int count, MPI_Datatype type, int root, MPI_Comm comm);
    void handle_n21(Caliper* c, int count, MPI_Datatype type, int root, MPI_Comm comm);
    void handle_n2n(Caliper* c, int count, MPI_Datatype type, MPI_Comm comm);    
    void handle_barrier(Caliper* c, MPI_Comm comm);
    void handle_init(Caliper* c);
    void handle_finalize(Caliper* c);
};

}

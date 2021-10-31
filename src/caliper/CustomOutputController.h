// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#ifndef CALI_CUSTOM_OUTPUT_CONTROLLER_H
#define CALI_CUSTOM_OUTPUT_CONTROLLER_H

#include "caliper/ChannelController.h"

#include <string>

namespace cali
{

class Aggregator;
class CaliperMetadataDB;
class OutputStream;

namespace internal
{

///   \brief Base class for a channel controller implementing a custom communication
/// scheme.
///
/// Lets us switch communication between serial, MPI, and potentially other
/// communication protocols.
class CustomOutputController : public cali::ChannelController
{
    typedef void (*FlushFn)(CustomOutputController*);
    static FlushFn s_flush_fn;

public:

    /// \brief Set the flush function for MPI (or other) communication
    ///   protocols.
    ///
    /// This callback lets us specify alternate flush implementations, in
    /// particular for MPI. The MPI build sets this in mpi_setup.cpp.
    static void set_flush_fn(FlushFn);

    ///   \brief Base class for the communication protocol. Implements serial
    /// (no-op) communication.
    class Comm {
    public:

        virtual ~Comm();

        virtual int rank() const;

        virtual int         bcast_int(int val) const;
        virtual std::string bcast_str(const std::string& str) const;

        virtual void cross_aggregate(CaliperMetadataDB& db, Aggregator& agg) const;
    };

    ///   \brief Override to implement custom collective flush operation using
    /// \a comm and \a stream.
    virtual void collective_flush(OutputStream& stream, Comm& comm) = 0;

    /// \brief Perform default flush (no user-provided comm or stream)
    ///
    ///   Invokes s_flush_fn() if set, which in turn invokes the collective_flush()
    /// method overriden by derived classes. The s_flush_fn() specifies the
    /// communication protocol, e.g. MPI. The default implementation is serial.
    virtual void flush() override;

    CustomOutputController(const char* name, int flags, const config_map_t& initial_config);
};

} // namespace internal

} // namespace cali

#endif

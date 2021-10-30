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
public:

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
    virtual void collective_flush(Comm& comm, OutputStream& stream) = 0;

    CustomOutputController(const char* name, int flags, const config_map_t& initial_config);
};

} // namespace internal

} // namespace cali

#endif

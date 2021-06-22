// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file CollectiveOutputChannel.h
/// CollectiveOutputChannel class

#pragma once

#ifndef CALI_COLLECTIVE_CHANNEL_CONTROLLER_H
#define CALI_COLLECTIVE_CHANNEL_CONTROLLER_H

#include "ChannelController.h"

#include <mpi.h>

#include <memory>

namespace cali
{

class OutputStream;

/// \class CollectiveOutputChannel
/// \ingroup ControlChannelAPI
/// \brief A ChannelController for %Caliper configurations that aggregate
///   output over MPI.
///
/// A CollectiveOutputChannel object controls a %Caliper measurement channel
/// that produces a single output in an MPI program. The output can be written
/// into a user-provided C++ I/O stream.
///
/// \sa make_collective_output_channel()
class CollectiveOutputChannel : public cali::ChannelController
{
    struct CollectiveOutputChannelImpl;
    std::shared_ptr<CollectiveOutputChannelImpl> mP;

public:

    /// \brief Create channel controller with given name, flags, and config.
    CollectiveOutputChannel(const char* name, int flags, const config_map_t& cfg);

    /// \brief Create channel controller with given queries, name, flags,
    ///   and config.
    CollectiveOutputChannel(const char* local_query,
                            const char* cross_query,
                            const char* name,
                            int flags,
                            const config_map_t& cfg);

    /// \brief Try to create a CollectiveOutputChannel based on the
    ///   configuration in \a from.
    ///
    /// Can be used to convert a control channel created by ConfigManager
    /// into a CollectiveOutputChannel object that can be flushed
    /// into a user-defined stream with collective_flush().
    ///
    /// Currently, this only works for input configurations that use the
    /// \e mpireport service.
    ///
    /// \return A new <tt>std::shared_ptr</tt>-wrapped CollectiveOutputChannel, or
    ///   an empty \c std::shared_ptr object if \a from couldn't be
    ///   made into a collective output channel.
    static std::shared_ptr<CollectiveOutputChannel>
    from(const std::shared_ptr<ChannelController>& from);

    /// \brief Aggregate data from MPI ranks in \a comm and write it into
    ///   \a os.
    ///
    /// This is a collective operation on the MPI communicator \a comm.
    /// Rank 0 in \a comm collects all output, and formats and writes it into
    /// \a os.
    /// If \a comm is \c MPI_COMM_NULL, the calling process will aggregate and
    /// write its local data.
    ///
    /// \param os Output stream object. Used only on rank 0 and ignored
    ///   on all other ranks.
    /// \param comm MPI communicator
    virtual void collective_flush(OutputStream& os, MPI_Comm comm);

    /// \copydoc CollectiveOutputChannel::collective_flush(OutputStream&, MPI_Comm)
    std::ostream& collective_flush(std::ostream& os, MPI_Comm comm);

    /// \brief Aggregate and flush data.
    ///
    /// The default behavior aggregates data across all ranks in the
    /// \c MPI_COMM_WORLD communicator and writes it to stdout. This is a
    /// collective operation on \c MPI_COMM_WORLD.
    ///
    /// If MPI was never initialized, the calling process will aggregate and
    /// write its local data. If MPI was initialized but has been finalized,
    /// the function does nothing.
    virtual void flush() override;

    virtual ~CollectiveOutputChannel();
};

/// \ingroup ControlChannelAPI
/// \brief Create a CollectiveOutputChannel object for a ConfigManager
///   configuration.
///
/// Returns a CollectiveOutputChannel object, which can be flushed into a
/// user-defined C++ stream, for the ConfigManager configuration in
/// \a config_str. If multiple configuration channels are given in
/// \a config_str, only the first one will be used.
///
/// Currently, this only works for input configurations that use the
/// \e mpireport service, such as \a runtime-report.
///
/// The example demonstrates how to collect and write \a runtime-report
/// output into a user-provided file stream:
/// \code
/// #include <caliper/CollectiveOutputChannel.h>
///
/// #include <mpi.h>
///
/// #include <fstream>
/// #include <iostream>
/// #include <tuple>
///
/// int main(int argc, char* argv[])
/// {
///     MPI_Init(&argc, &argv);
///
///     std::shared_ptr<cali::CollectiveOutputChannel> channel;
///     std::string errmsg;
///
///     std::tie(channel, errmsg) =
///         cali::make_collective_output_channel("runtime-report(profile.mpi)");
///
///     if (!channel) {
///         std::cerr << "Error: " << errmsg << std::endl;
///         MPI_Abort(MPI_COMM_WORLD, -1);
///     }
///
///     channel->start();
///
///     CALI_MARK_BEGIN("work");
///     // ...
///     CALI_MARK_END("work");
///
///     int rank = 0;
///     MPI_Comm_rank(MPI_COMM_WORLD, &rank);
///
///     std::ofstream os;
///     if (rank == 0) {
///         os.open("report.txt");
///     }
///
///     // collective_flush() is collective on MPI_COMM_WORLD
///     channel->collective_flush(os, MPI_COMM_WORLD);
///
///     MPI_Finalize();
/// }
/// \endcode
///
/// \return A \c std::pair where the first element is the
///   <tt>std::shared_ptr</tt>-wrapped CollectiveOutputChannel object for the
///   given configuration. If the \c std::shared_ptr object is empty, an error
///   occured and the second pair element contains an error message.
std::pair< std::shared_ptr<CollectiveOutputChannel>, std::string >
make_collective_output_channel(const char* config_str);

} // namespace cali

#endif

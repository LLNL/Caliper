// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/MpiChannelManager.h"

#include "caliper/CollectiveOutputChannel.h"
#include "caliper/ConfigManager.h"

#include <vector>

using namespace cali;


struct MpiChannelManager::MpiChannelManagerImpl
{
    MPI_Comm comm       { MPI_COMM_NULL };
    bool     is_in_comm { false };

    // mpi channels are those using collective_flush()
    std::vector< std::shared_ptr<CollectiveOutputChannel> > mpi_channels;
    // serial channels just use flush()
    std::vector< std::shared_ptr<ChannelController      > > ser_channels;

    void add(const std::shared_ptr<ChannelController>& src) {
        if (!src)
            return;

        auto mpi_chn = CollectiveOutputChannel::from(src);

        if (mpi_chn)
            mpi_channels.push_back(mpi_chn);
        else
            ser_channels.push_back(src);
    }

    void start() const {
        if (!is_in_comm)
            return;

        for (auto channel : mpi_channels)
            channel->start();
        for (auto channel : ser_channels)
            channel->start();
    }

    void stop() const {
        if (!is_in_comm)
            return;

        for (auto channel : mpi_channels)
            channel->stop();
        for (auto channel : ser_channels)
            channel->stop();
    }

    void flush() const {
        if (!is_in_comm)
            return;

        for (auto channel : mpi_channels)
            channel->collective_flush(comm);
        for (auto channel : ser_channels)
            channel->flush();
    }

    MpiChannelManagerImpl(MPI_Comm comm_)
        : comm { comm_ }
    {
        MPI_Group group;
        MPI_Comm_group(comm, &group);
        int rank;
        MPI_Group_rank(group, &rank);
        is_in_comm = (rank != MPI_UNDEFINED);
    }
};


MpiChannelManager::MpiChannelManager(MPI_Comm comm)
    : mP { new MpiChannelManagerImpl(comm) }
{ }

MpiChannelManager::~MpiChannelManager()
{ }

void MpiChannelManager::add(const std::shared_ptr<ChannelController>& src)
{
    mP->add(src);
}

void MpiChannelManager::add(const ConfigManager& mgr)
{
    auto channels = mgr.get_all_channels();
    for (auto c : channels)
        mP->add(c);
}

void MpiChannelManager::start()
{
    mP->start();
}

void MpiChannelManager::stop()
{
    mP->stop();
}

void MpiChannelManager::collective_flush()
{
    mP->flush();
}

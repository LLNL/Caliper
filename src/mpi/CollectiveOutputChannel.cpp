// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CollectiveOutputChannel.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/StringConverter.h"

#include "caliper/reader/CalQLParser.h"

#include "caliper/Caliper.h"
#include "caliper/ConfigManager.h"

#include "caliper/cali-mpi.h"

#include <algorithm>
#include <iostream>

using namespace cali;


namespace
{

std::string remove_from_stringlist(const std::string& in, const std::string& element)
{
    auto vec = StringConverter(in).to_stringlist();

    // delete all occurences of element in the vector
    for (auto pos = std::find(vec.begin(), vec.end(), element); pos != vec.end(); pos = std::find(vec.begin(), vec.end(), element))
        vec.erase(pos);

    std::string ret;
    int count = 0;

    for (const auto &s : vec) {
        if (count++ > 0)
            ret.append(",");
        ret.append(s);
    }

    return ret;
}

}


struct CollectiveOutputChannel::CollectiveOutputChannelImpl
{
    std::string local_query;
    std::string cross_query;
};


void CollectiveOutputChannel::collective_flush(OutputStream& stream, MPI_Comm comm)
{
    Channel* chn = channel();

    if (!chn || !mP)
        return;

    QuerySpec cross_spec;
    QuerySpec local_spec;

    {
        CalQLParser p(mP->cross_query.c_str());

        if (p.error()) {
            Log(0).stream() << "CollectiveOutputChannel: cross query parse error: "
                            << p.error_msg()
                            << std::endl;
            return;
        } else {
            cross_spec = p.spec();
        }
    }

    {
        CalQLParser p(mP->local_query.c_str());

        if (p.error()) {
            Log(0).stream() << "CollectiveOutputChannel: local query parse error: "
                            << p.error_msg()
                            << std::endl;
            return;
        } else {
            local_spec = p.spec();
        }
    }

    Caliper c;
    cali::collective_flush(stream, c, *chn, nullptr, local_spec, cross_spec, comm);
}

std::ostream& CollectiveOutputChannel::collective_flush(std::ostream& os, MPI_Comm comm)
{
    OutputStream stream;
    stream.set_stream(&os);
    collective_flush(stream, comm);
    return os;
}

void CollectiveOutputChannel::flush()
{
    int initialized = 0;
    int finalized = 0;

    MPI_Comm comm = MPI_COMM_NULL;

    MPI_Initialized(&initialized);
    MPI_Finalized(&finalized);

    if (finalized)
        return;
    if (initialized)
        MPI_Comm_dup(MPI_COMM_WORLD, &comm);

    collective_flush(std::cout, comm);

    if (comm != MPI_COMM_NULL)
        MPI_Comm_free(&comm);
}

std::shared_ptr<CollectiveOutputChannel>
CollectiveOutputChannel::from(const std::shared_ptr<ChannelController>& from)
{
    config_map_t cfg(from->copy_config());

    cfg["CALI_SERVICES_ENABLE"] = ::remove_from_stringlist(cfg["CALI_SERVICES_ENABLE"], "mpireport");

    std::string cross_query = cfg["CALI_MPIREPORT_CONFIG"];
    std::string local_query = cfg["CALI_MPIREPORT_LOCAL_CONFIG"];

    if (cross_query.empty())
        return { nullptr };
    if (local_query.empty())
        local_query = cross_query;

    std::string name = from->name() + ".output";

    return std::make_shared<CollectiveOutputChannel>(local_query.c_str(), cross_query.c_str(), name.c_str(), 0, cfg);
}

CollectiveOutputChannel::CollectiveOutputChannel(const char* name, int flags, const config_map_t& cfg)
    : ChannelController(name, flags, cfg),
      mP(nullptr)
{ }

CollectiveOutputChannel::CollectiveOutputChannel(const char* local_query,
                                                 const char* cross_query,
                                                 const char* name,
                                                 int flags,
                                                 const config_map_t& cfg)
    : ChannelController(name, flags, cfg),
      mP(new CollectiveOutputChannelImpl)
{
    mP->cross_query = cross_query;
    mP->local_query = local_query;
}

CollectiveOutputChannel::~CollectiveOutputChannel()
{ }

namespace cali
{

std::pair< std::shared_ptr<CollectiveOutputChannel>, std::string >
make_collective_output_channel(const char* config_str)
{
    ConfigManager mgr;
    auto configs = mgr.parse(config_str);

    if (mgr.error())
        return std::make_pair(nullptr, mgr.error_msg());
    if (configs.empty())
        return std::make_pair(nullptr, "No config specified");

    auto ret = CollectiveOutputChannel::from(configs.front());

    if (!ret) {
        std::string msg("Cannot create CollectiveOutputChannel from ");
        msg.append(configs.front()->name());

        return std::make_pair(nullptr, msg);
    }

    return std::make_pair(ret, std::string());
}

} // namespace cali

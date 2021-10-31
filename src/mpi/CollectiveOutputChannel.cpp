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

#include "../caliper/CustomOutputController.h"

#include "OutputCommMpi.h"

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

class MpiReportWrapper : public CollectiveOutputChannel
{
    std::shared_ptr<cali::ChannelController> m_channel;

    std::string m_local_query;
    std::string m_cross_query;

public:

    /// \brief Create channel controller with given queries, name, flags,
    ///   and config.
    MpiReportWrapper(const std::string& local_query,
                     const std::string& cross_query,
                     const char* name,
                     int flags,
                     const config_map_t& cfg)
        : m_channel(std::make_shared<ChannelController>(name, flags, cfg)),
          m_local_query(local_query),
          m_cross_query(cross_query)
    { }

    void start() override {
        m_channel->start();
    }

    void stop() override {
        m_channel->stop();
    }

    void collective_flush(OutputStream& stream, MPI_Comm comm) override {
        Channel* chn = m_channel->channel();

        if (!chn)
            return;

        QuerySpec cross_spec;
        QuerySpec local_spec;

        {
            CalQLParser p(m_cross_query.c_str());

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
            CalQLParser p(m_local_query.c_str());

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

    void collective_flush(MPI_Comm comm) override {
        OutputStream stream;

        auto cfg = m_channel->copy_config();
        auto it = cfg.find("CALI_MPIREPORT_FILENAME");
        if (it == cfg.end()) {
            stream.set_stream(OutputStream::StdOut);
        } else {
            Caliper c;
            stream.set_filename(it->second.c_str(), c, c.get_globals());
        }

        collective_flush(stream, comm);
    }
};

class CustomOutputControllerWrapper : public CollectiveOutputChannel
{
    std::shared_ptr<cali::internal::CustomOutputController> m_channel;

public:

    CustomOutputControllerWrapper(std::shared_ptr<cali::internal::CustomOutputController>& from)
        : m_channel(from)
    { }

    void start() override {
        m_channel->start();
    }

    void stop() override {
        m_channel->stop();
    }

    void collective_flush(OutputStream& stream, MPI_Comm mpi_comm) override {
        OutputCommMpi comm(mpi_comm);
        m_channel->collective_flush(stream, comm);
    }
};

} // namespace [anonymous]


std::ostream& CollectiveOutputChannel::collective_flush(std::ostream& os, MPI_Comm comm)
{
    OutputStream stream;
    stream.set_stream(&os);
    collective_flush(stream, comm);
    return os;
}

void CollectiveOutputChannel::collective_flush(MPI_Comm comm)
{
    OutputStream stream;
    collective_flush(stream, comm);
}

std::shared_ptr<CollectiveOutputChannel>
CollectiveOutputChannel::from(const std::shared_ptr<ChannelController>& from)
{
    // if from is already a CollectiveOutputChannel, just return it

    {
        auto cast = std::dynamic_pointer_cast<cali::internal::CustomOutputController>(from);

        if (cast)
            return std::make_shared<::CustomOutputControllerWrapper>(cast);
    }

    //   if from uses mpireport, make a BasicCollectiveOutputChannel from
    // its config

    config_map_t cfg(from->copy_config());

    cfg["CALI_SERVICES_ENABLE"] = ::remove_from_stringlist(cfg["CALI_SERVICES_ENABLE"], "mpireport");
    cfg["CALI_CHANNEL_CONFIG_CHECK" ] = "false";
    cfg["CALI_CHANNEL_FLUSH_AT_EXIT"] = "false";

    std::string cross_query = cfg["CALI_MPIREPORT_CONFIG"];
    std::string local_query = cfg["CALI_MPIREPORT_LOCAL_CONFIG"];

    if (cross_query.empty())
        return { nullptr };
    if (local_query.empty())
        local_query = cross_query;

    std::string name = from->name() + ".output";

    return std::make_shared<::MpiReportWrapper>(local_query, cross_query, name.c_str(), 0, cfg);
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

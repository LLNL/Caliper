// Copyright (c) 2015-2023, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/ChannelController.h"
#include "caliper/ConfigManager.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <array>
#include <chrono>
#include <vector>

using namespace cali;

namespace
{

inline double get_timestamp()
{
    return 1e-6
           * std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
                 .count();
}

class TimeseriesService
{
    Channel   m_channel;

    Attribute m_timestamp_attr;
    Attribute m_snapshot_attr;
    Attribute m_duration_attr;

    ConfigManager::ChannelPtr m_timeprofile;

    unsigned m_snapshots;

    static const char* s_profile_spec;

    void snapshot_cb(Caliper* c, SnapshotView info, SnapshotBuilder& srec)
    {
        double  ts_now = get_timestamp();
        Variant v_prev = c->exchange(m_timestamp_attr, Variant(ts_now));

        std::array<Entry, 2> ts_entries = { Entry(m_timestamp_attr, v_prev),
                                            Entry(m_snapshot_attr, cali_make_variant_from_uint(m_snapshots)) };

        Channel prof_chn = m_timeprofile->channel();

        c->flush(
            prof_chn.body(),
            info,
            [this, c, info, ts_entries](CaliperMetadataAccessInterface&, const std::vector<Entry>& frec) {
                std::vector<Entry> rec;
                rec.reserve(frec.size() + info.size() + ts_entries.size());
                rec.insert(rec.end(), frec.begin(), frec.end());
                rec.insert(rec.end(), info.begin(), info.end());
                rec.insert(rec.end(), ts_entries.begin(), ts_entries.end());

                m_channel.events().process_snapshot(c, SnapshotView(), SnapshotView(rec.size(), rec.data()));
            }
        );

        c->clear(&prof_chn);

        srec.append(ts_entries.size(), ts_entries.data());
        srec.append(m_duration_attr, Variant(ts_now - v_prev.to_double()));

        ++m_snapshots;
    }

    void post_init_cb(Caliper* c, Channel*)
    {
        c->set(m_timestamp_attr, Variant(get_timestamp()));
        m_timeprofile->start();
    }

    void finish_cb(Caliper*, Channel* channel)
    {
        Log(1).stream() << channel->name() << ": timeseries: Processed " << m_snapshots << " snapshots\n";
    }

    TimeseriesService(Caliper* c, Channel* channel, ConfigManager::ChannelPtr prof)
        : m_channel { *channel }, m_timeprofile { prof }, m_snapshots { 0 }
    {
        m_timestamp_attr =
            c->create_attribute("timeseries.starttime", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
        m_snapshot_attr =
            c->create_attribute("timeseries.snapshot", CALI_TYPE_UINT, CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
        m_duration_attr = c->create_attribute(
            "timeseries.duration",
            CALI_TYPE_DOUBLE,
            CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE
        );
    }

public:

    static const char* s_spec;

    static void create(Caliper* c, Channel* channel)
    {
        std::string profile_cfg_str = "timeseries.profile";

        ConfigSet   cfg          = services::init_config_from_spec(channel->config(), s_spec);
        std::string profile_opts = cfg.get("profile.options").to_string();
        if (profile_opts.size() > 0)
            profile_cfg_str.append("(").append(profile_opts).append(")");

        ConfigManager mgr;
        mgr.add_config_spec(s_profile_spec);
        mgr.add(profile_cfg_str.c_str());

        if (mgr.error()) {
            Log(0).stream() << channel->name() << ": timeseries: Profile config error: " << mgr.error_msg() << "\n";
            return;
        }

        auto profile = mgr.get_channel("timeseries.profile");
        if (!profile) {
            Log(0).stream() << channel->name() << ": timeseries: Cannot create profile channel\n";
            return;
        }

        TimeseriesService* instance = new TimeseriesService(c, channel, profile);

        channel->events().snapshot.connect(
            [instance](Caliper* c, SnapshotView info, SnapshotBuilder& rec) {
                instance->snapshot_cb(c, info, rec);
            }
        );
        channel->events().post_init_evt.connect([instance](Caliper* c, Channel* channel) {
            instance->post_init_cb(c, channel);
        });
        channel->events().finish_evt.connect([instance](Caliper* c, Channel* channel) {
            instance->finish_cb(c, channel);
            delete instance;
        });

        Log(1).stream() << channel->name() << ": Registered timeseries service\n";
    }
};

const char* TimeseriesService::s_profile_spec = R"json(
{
 "name"        : "timeseries.profile",
 "description" : "Runtime profile for timeseries service",
 "categories"  : [ "region", "metric", "event" ],
 "services"    : [ "aggregate", "event", "timer" ],
 "config":
 {
   "CALI_CHANNEL_FLUSH_ON_EXIT"      : "false",
   "CALI_EVENT_ENABLE_SNAPSHOT_INFO" : "false",
   "CALI_AGGREGATE_KEY"              : "*,mpi.rank"
 }
}
)json";

const char* TimeseriesService::s_spec = R"json(
{
 "name"        : "timeseries",
 "description" : "Run a sub-profile for time series profiling",
 "config"      :
 [
  { "name"        : "profile_options",
    "type"        : "string",
    "description" : "Extra config options for the iteration sub-profiles"
  }
 ]
}
)json";

} // namespace

namespace cali
{

CaliperService timeseries_service { ::TimeseriesService::s_spec, ::TimeseriesService::create };

}

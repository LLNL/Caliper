// Copyright (c) 2015-2023, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/ChannelController.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <chrono>
#include <vector>

using namespace cali;

namespace
{

inline double get_timestamp()
{
    return 1e-6 * std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class TimeseriesService
{
    Attribute m_timestamp_attr;
    Attribute m_snapshot_attr;
    Attribute m_duration_attr;

    ChannelController m_timeprofile;

    unsigned  m_snapshots;

    void snapshot_cb(Caliper* c, Channel* channel, SnapshotView info, SnapshotBuilder& srec) {
        double ts_now = get_timestamp();
        Variant v_prev = c->exchange(m_timestamp_attr, Variant(ts_now));

        std::array<Entry, 2> ts_entries = {
            Entry(m_timestamp_attr, v_prev),
            Entry(m_snapshot_attr, cali_make_variant_from_uint(m_snapshots))
        };

        Channel* prof_chn = m_timeprofile.channel();

        c->flush(prof_chn, info, [c,channel,info,ts_entries](CaliperMetadataAccessInterface&, const std::vector<Entry>& frec){
                std::vector<Entry> rec;
                rec.reserve(frec.size() + info.size() + ts_entries.size());
                rec.insert(rec.end(), frec.begin(), frec.end());
                rec.insert(rec.end(), info.begin(), info.end());
                rec.insert(rec.end(), ts_entries.begin(), ts_entries.end());

                channel->events().process_snapshot(c, channel, SnapshotView(), SnapshotView(rec.size(), rec.data()));
            });

        c->clear(prof_chn);

        srec.append(ts_entries.size(), ts_entries.data());
        srec.append(m_duration_attr, Variant(ts_now - v_prev.to_double()));

        ++m_snapshots;
    }

    void post_init_cb(Caliper* c, Channel*) {
        c->set(m_timestamp_attr, Variant(get_timestamp()));
        m_timeprofile.start();
    }

    void finish_cb(Caliper*, Channel* channel) {
        Log(1).stream() << channel->name() << ": timeseries: Processed "
            << m_snapshots << " snapshots\n";
    }

    TimeseriesService(Caliper* c, Channel* channel)
        : m_timeprofile("timeseries.timeprofile", CALI_CHANNEL_LEAVE_INACTIVE,
            {   { "CALI_CHANNEL_FLUSH_ON_EXIT",      "false" },
                { "CALI_CHANNEL_CONFIG_CHECK",       "false" },
                { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false" },
                { "CALI_SERVICES_ENABLE", "aggregate,event,timer" },
                { "CALI_AGGREGATE_KEY",   "*,mpi.rank" }
            }),
            m_snapshots(0)
    {
        m_timestamp_attr =
            c->create_attribute("timeseries.starttime", CALI_TYPE_DOUBLE,
                                CALI_ATTR_ASVALUE |
                                CALI_ATTR_SKIP_EVENTS);
        m_snapshot_attr =
            c->create_attribute("timeseries.snapshot", CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE |
                                CALI_ATTR_SKIP_EVENTS);
        m_duration_attr =
            c->create_attribute("timeseries.duration", CALI_TYPE_DOUBLE,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_AGGREGATABLE);
    }

public:

    static const char* s_spec;

    static void create(Caliper* c, Channel* channel) {
        TimeseriesService* instance = new TimeseriesService(c, channel);

        channel->events().snapshot.connect(
            [instance](Caliper* c, Channel* channel, SnapshotView info, SnapshotBuilder& rec){
                instance->snapshot_cb(c, channel, info, rec);
            });
        channel->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->post_init_cb(c, channel);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish_cb(c, channel);
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered timeseries service\n";
    }
};

const char* TimeseriesService::s_spec = R"json(
{
    "name"        : "timeseries",
    "description" : "Run a sub-profile for time series profiling"
}
)json";

} // namespace [anonymous]

namespace cali
{

CaliperService timeseries_service { ::TimeseriesService::s_spec, ::TimeseriesService::create };

}

// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <chrono>
#include <vector>

using namespace cali;

namespace cali
{

extern Attribute class_iteration_attr;
extern Attribute loop_attr;

}

namespace
{

class LoopStatisticsService
{
    using clock = std::chrono::steady_clock;

    struct LoopInfo {
        clock::time_point iter_start_time;
        uint64_t num_iterations;
    };

    std::vector<LoopInfo> m_loop_info;

    Attribute m_iter_duration_attr;
    Attribute m_iter_count_attr;

    void begin_cb(Caliper* c, Channel* channel, const Attribute& attr, const Variant& data) {
        if (attr == loop_attr) {
            m_loop_info.emplace_back(LoopInfo { clock::now(), 0 });
        } else if (attr.get(class_iteration_attr).to_bool() && !m_loop_info.empty()) {
            m_loop_info.back().iter_start_time = clock::now();
            m_loop_info.back().num_iterations++;
        }
    }

    void end_cb(Caliper* c, Channel* channel, const Attribute& attr, const Variant& data) {
        if (m_loop_info.empty())
            return;
        if (attr == loop_attr) {
            Entry e { m_iter_count_attr, Variant(m_loop_info.back().num_iterations) };
            c->push_snapshot(channel, SnapshotView(e));
            m_loop_info.pop_back();
        } else if (attr.get(class_iteration_attr).to_bool()) {
            uint64_t t = std::chrono::duration_cast<std::chrono::nanoseconds>(
                clock::now() - m_loop_info.back().iter_start_time).count();
            Entry e { m_iter_duration_attr, Variant(t) };
            c->push_snapshot(channel, SnapshotView(e));
        }
    }

    LoopStatisticsService(Caliper* c, Channel* channel)
    {
        m_iter_duration_attr =
            c->create_attribute("iter.duration.ns", CALI_TYPE_UINT,
                CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE | CALI_ATTR_SKIP_EVENTS);
        m_iter_count_attr =
            c->create_attribute("iter.count", CALI_TYPE_UINT,
                CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE | CALI_ATTR_SKIP_EVENTS);

        m_loop_info.reserve(4);
    }

public:

    static const char* s_spec;

    static void create(Caliper* c, Channel* channel) {
        LoopStatisticsService* instance = new LoopStatisticsService(c, channel);

        channel->events().pre_begin_evt.connect(
            [instance](Caliper* c, Channel* channel, const Attribute& attr, const Variant& data){
                    instance->begin_cb(c, channel, attr, data);
                });
        channel->events().pre_end_evt.connect(
            [instance](Caliper* c, Channel* channel, const Attribute& attr, const Variant& data){
                    instance->end_cb(c, channel, attr, data);
                });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                    delete instance;
                });

        Log(1).stream() << channel->name() << ": registered loop_statistics service\n";
    }
};

const char* LoopStatisticsService::s_spec = R"json(
{
 "name": "loop_statistics",
 "description": "Record loop iteration statistics"
}
)json";

} // namespace [anonymous]

namespace cali
{

CaliperService loop_statistics_service { ::LoopStatisticsService::s_spec, ::LoopStatisticsService::create };

}
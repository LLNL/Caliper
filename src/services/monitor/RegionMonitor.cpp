// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <chrono>
#include <unordered_map>

using namespace cali;

namespace cali
{

extern Attribute class_aggregatable_attr;
extern Attribute class_iteration_attr;

}

namespace
{

class RegionMonitor
{
    static const ConfigSet::Entry s_configdata[];

    struct RegionInfo {
        double inclusive_time;
        double child_time;
    };

    std::unordered_map<cali_id_t, RegionInfo> m_tracking_regions;
    std::vector<std::chrono::high_resolution_clock::time_point> m_time_stack;

    double   m_min_interval;
    bool     m_measuring;
    int      m_skip;

    unsigned m_num_measured;

    void post_begin_cb(Caliper* c, Channel* channel, const Attribute& attr, const Variant&) {
        if (!attr.is_nested())
            return;
        if (m_measuring) {
            ++m_skip;
            return;
        }

        const Node* node = c->get(attr).node();
        if (!node)
            return;

        m_time_stack.push_back(std::chrono::high_resolution_clock::now());

        auto it = m_tracking_regions.find(node->id());

        if (it != m_tracking_regions.end()) {
            if (it->second.inclusive_time > 2.0 * it->second.child_time) {
                SnapshotRecord rec;
                c->pull_snapshot(channel, CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, nullptr, &rec);
                m_measuring = true;
                m_skip = 1;
            }
        }
    }

    void pre_end_cb(Caliper* c, Channel* channel, const Attribute& attr, const Variant&) {
        if (!attr.is_nested())
            return;

        if (m_measuring) {
            if (--m_skip > 0)
                return;

            m_measuring = false;
            ++m_num_measured;
            c->push_snapshot(channel, nullptr);
        }

        const Node* node = c->get(attr).node();
        if (!node)
            return;

        auto now  = std::chrono::high_resolution_clock::now();
        auto prev = m_time_stack.back();
        m_time_stack.pop_back();

        double duration = std::chrono::duration<double>(now - prev).count();

        if (duration > m_min_interval) {
            auto it = m_tracking_regions.find(node->id());

            if (it != m_tracking_regions.end())
                it->second.inclusive_time = duration;
            else {
                RegionInfo reg { duration, 0.0 };
                m_tracking_regions.insert(std::make_pair(node->id(), reg));
            }

            node = node->parent();

            if (!node || node->id() == CALI_INV_ID)
                return;

            it = m_tracking_regions.find(node->id());

            if (it != m_tracking_regions.end())
                it->second.child_time += duration;
            else {
                RegionInfo reg { 0.0, duration };
                m_tracking_regions.insert(std::make_pair(node->id(), reg));
            }
        }
    }

    void finish_cb(Caliper*, Channel* channel) {
        Log(1).stream() << channel->name()
                        << ": " << m_tracking_regions.size()
                        << " regions marked, "
                        << m_num_measured << " instances measured."
                        << std::endl;
    }

    RegionMonitor(Caliper*, Channel* channel)
        : m_measuring(false),
          m_skip(0),
          m_num_measured(0)
        {
            ConfigSet config = channel->config().init("region_monitor", s_configdata);

            m_min_interval = config.get("time_interval").to_double();
        }

public:

    static void create(Caliper* c, Channel* channel) {
        RegionMonitor* instance = new RegionMonitor(c, channel);

        channel->events().post_begin_evt.connect(
            [instance](Caliper* c, Channel* channel, const Attribute& attr, const Variant& val) {
                instance->post_begin_cb(c, channel, attr, val);
            });
        channel->events().pre_end_evt.connect(
            [instance](Caliper* c, Channel* channel, const Attribute& attr, const Variant& val) {
                instance->pre_end_cb(c, channel, attr, val);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish_cb(c, channel);
                delete instance;
            });

        Log(1).stream() << channel->name()
                        << ": Registered region_monitor service"
                        << std::endl;
    }
};

const ConfigSet::Entry RegionMonitor::s_configdata[] = {
    { "time_interval",     CALI_TYPE_DOUBLE, "0.01",
      "Minimum time for regions to measure."
      "Minimum time for regions to measure."
    },
    ConfigSet::Terminator
};

}

namespace cali
{

CaliperService region_monitor_service { "region_monitor", ::RegionMonitor::create };

}

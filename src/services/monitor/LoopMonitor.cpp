// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "../Services.h"

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

class LoopMonitor
{
    int       loop_level;
    int       target_level;
    int       start_iteration;
    int       num_iterations;
    int       num_snapshots;

    int       iteration_interval;
    double    time_interval;

    Attribute num_iterations_attr;
    Attribute start_iteration_attr;

    std::vector<std::string> target_loops;

    std::chrono::high_resolution_clock::time_point last_snapshot_time;

    bool is_target_loop(const Variant& value) {
        if (target_loops.empty())
            return true;

        for (const std::string& s : target_loops)
            if (strncmp(static_cast<const char*>(value.data()), s.data(), s.size()) == 0)
                return true;

        return false;
    }

    void snapshot(Caliper* c, Channel* channel) {
        Entry data[] = {
            { num_iterations_attr,  Variant(num_iterations)  },
            { start_iteration_attr, Variant(start_iteration) }
        };
        size_t n = start_iteration >= 0 ? 2 : 1;
        c->push_snapshot(channel, SnapshotView(n, data));

        start_iteration = -1;
        num_iterations  =  0;
        ++num_snapshots;

        last_snapshot_time = std::chrono::high_resolution_clock::now();
    }

    void begin_cb(Caliper* c, Channel* channel, const Attribute& attr, const Variant& value) {
        if (attr == loop_attr) {
            if (target_level < 0 && is_target_loop(value)) {
                target_level = loop_level + 1;
                snapshot(c, channel);
            }
            ++loop_level;
        } else if (loop_level == target_level && attr.get(cali::class_iteration_attr).to_bool()) {
            ++num_iterations;
            if (start_iteration < 0)
                start_iteration = value.to_int();
        }
    }

    void end_cb(Caliper* c, Channel* channel, const Attribute& attr, const Variant& value) {
        if (attr == loop_attr) {
            if (loop_level == target_level) {
                snapshot(c, channel);
                target_level = -1;
            }
            --loop_level;
        } else if (loop_level == target_level && attr.get(cali::class_iteration_attr).to_bool()) {
            bool do_snapshot = false;

            if (iteration_interval > 0 && num_iterations % iteration_interval == 0)
                do_snapshot = true;
            if (time_interval > 0) {
                auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration<double>(now - last_snapshot_time).count() > time_interval)
                    do_snapshot = true;
            }

            if (do_snapshot)
                snapshot(c, channel);
        }
    }

    void finish_cb(Caliper* c, Channel* channel) {
        Log(1).stream() << channel->name()
                        << ": loop_monitor: Triggered " << num_snapshots << " snapshots."
                        << std::endl;
    }

    LoopMonitor(Caliper* c, Channel* channel)
        : loop_level(0),
          target_level(-1),
          start_iteration(-1),
          num_iterations(0),
          num_snapshots(0),
          iteration_interval(0),
          time_interval(0.0)
    {
        Variant v_true(true);

        num_iterations_attr =
            c->create_attribute("loop.iterations", CALI_TYPE_INT,
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_AGGREGATABLE);
        start_iteration_attr =
            c->create_attribute("loop.start_iteration", CALI_TYPE_INT,
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_ASVALUE);

        ConfigSet config = services::init_config_from_spec(channel->config(), s_spec);

        iteration_interval = config.get("iteration_interval").to_int();
        time_interval      = config.get("time_interval").to_double();
        target_loops       = config.get("target_loops").to_stringlist();
    }

public:

    static const char* s_spec;

    static void create(Caliper* c, Channel* channel) {
        LoopMonitor* instance = new LoopMonitor(c, channel);

        channel->events().pre_begin_evt.connect(
            [instance](Caliper* c, Channel* channel, const Attribute& attr, const Variant& val) {
                instance->begin_cb(c, channel, attr, val);
            });
        channel->events().pre_end_evt.connect(
            [instance](Caliper* c, Channel* channel, const Attribute& attr, const Variant& val) {
                instance->end_cb(c, channel, attr, val);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish_cb(c, channel);
                delete instance;
            });

        Log(1).stream() << channel->name()
                        << ": Registered loop_monitor service"
                        << std::endl;
    }
};

const char* LoopMonitor::s_spec = R"json(
{   "name"        : "loop_monitor",
    "description" : "Trigger snapshots on loop iterations",
    "config"      : [
        {   "name"        : "iteration_interval",
            "description" : "Trigger snapshots every N iterations",
            "type"        : "int",
            "value"       : "0"
        },
        {   "name"        : "time_interval",
            "description" : "Trigger snapshots every t seconds",
            "type"        : "double",
            "value"       : "0.5"
        },
        {   "name"        : "target_loops",
            "description" : "List of loops to instrument",
            "type"        : "string"
        }
    ]
}
)json";

} // namespace [anonymous]

namespace cali
{

CaliperService loop_monitor_service { ::LoopMonitor::s_spec, ::LoopMonitor::create };

}

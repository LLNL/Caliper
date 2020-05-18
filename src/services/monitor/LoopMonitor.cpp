// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <time.h>

using namespace cali;

namespace cali
{

extern Attribute class_aggregatable_attr;
extern Attribute class_iteration_attr;
extern Attribute loop_attr;

}

namespace
{

class LoopMonitor
{
    static const ConfigSet::Entry s_configdata[];

    int       loop_level;
    int       num_iterations;
    int       num_snapshots;

    int       iteration_interval;

    Attribute num_iterations_attr;

    void snapshot(Caliper* c, Channel* channel) {
        cali_id_t attr_id = num_iterations_attr.id();
        Variant   v_iter(num_iterations);

        SnapshotRecord trigger_info(1, &attr_id, &v_iter);
        c->push_snapshot(channel, &trigger_info);

        num_iterations = 0;
        ++num_snapshots;
    }

    void begin_cb(Caliper* c, Channel* channel, const Attribute& attr, const Variant& value) {
        if (attr == loop_attr) {
            snapshot(c, channel);
            ++loop_level;
        } else if (loop_level == 1 && attr.get(cali::class_iteration_attr).to_bool())
            ++num_iterations;
    }

    void end_cb(Caliper* c, Channel* channel, const Attribute& attr, const Variant& value) {
        if (attr == loop_attr) {
            if (loop_level == 1)
                snapshot(c, channel);
            --loop_level;
        } else if (loop_level == 1 && attr.get(cali::class_iteration_attr).to_bool()) {
            if (iteration_interval > 0 && num_iterations % iteration_interval == 0)
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
          num_iterations(0),
          num_snapshots(0),
          iteration_interval(0)
    {
        Variant v_true(true);

        num_iterations_attr =
            c->create_attribute("loop.iterations", CALI_TYPE_INT, CALI_ATTR_ASVALUE,
                                1, &class_aggregatable_attr, &v_true);

        ConfigSet config = channel->config().init("loop_monitor", s_configdata);
        iteration_interval = config.get("iteration_interval").to_int();
    }

public:

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

const ConfigSet::Entry LoopMonitor::s_configdata[] = {
    { "iteration_interval", CALI_TYPE_INT, "0",
      "Trigger snapshot every N iterations."
      "Trigger snapshot every N iterations. Set to 0 to disable."
    },
    ConfigSet::Terminator
};

} // namespace [anonymous]

namespace cali
{

CaliperService loop_monitor_service { "loop_monitor", ::LoopMonitor::create };

}
// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <pthread.h>
#include <time.h>

using namespace cali;

namespace
{

class ThreadMonitor
{
    static const ConfigSet::Entry s_configdata[];

    pthread_t monitor_thread;
    bool      thread_running;

    Channel*  channel;

    time_t    sleep_sec;
    long      sleep_nsec;

    Attribute monitor_attr;

    int       num_events;

    void snapshot() {
        Caliper c;

        cali_id_t attr_id = monitor_attr.id();
        Variant   v_event(cali_make_variant_from_int(num_events++));

        SnapshotRecord trigger_info(1, &attr_id, &v_event);
        c.push_snapshot(channel, &trigger_info);
    }

    // the main monitor loop
    void monitor() {
        struct timespec req { sleep_sec, sleep_nsec };

        while (true) {
            struct timespec rem = req;
            int ret = 0;

            do {
                ret = nanosleep(&rem, &rem);
            } while (ret == EINTR);

            if (ret != 0) {
                Log(0).perror(errno, "monitor: nanosleep(): ");
                break;
            }

            // disable cancelling during snapshot
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
            snapshot();
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        }
    }

    static void* thread_fn(void* p) {
        ThreadMonitor* instance = static_cast<ThreadMonitor*>(p);
        instance->monitor();
        return nullptr;
    }

    bool start() {
        if (pthread_create(&monitor_thread, nullptr, thread_fn, this) != 0) {
            Log(0).stream() << channel->name() << ": monitor(): pthread_create() failed"
                            << std::endl;
            return false;
        }

        Log(1).stream() << channel->name() << ": monitor: monitoring thread initialized" << std::endl;
        thread_running = true;
        return true;
    }

    void cancel() {
        Log(2).stream() << channel->name() << ": monitor: cancelling monitoring thread" << std::endl;

        if (pthread_cancel(monitor_thread) != 0)
            Log(0).stream() << channel->name() << ": monitor: pthread_cancel() failed" << std::endl;

        pthread_join(monitor_thread, nullptr);

        Log(1).stream() << channel->name() << ": monitor: monitoring thread finished" << std::endl;
    }

    void post_init_cb() {
        if (!start())
            return;
    }

    void finish_cb() {
        if (thread_running)
            cancel();

        Log(1).stream() << channel->name()
                        << ": monitor: triggered " << num_events << " monitoring events"
                        << std::endl;
    }

    ThreadMonitor(Caliper* c, Channel* chn)
        : thread_running(false), channel(chn), sleep_sec(2), sleep_nsec(0), num_events(0)
        {
            monitor_attr =
                c->create_attribute("monitor.event", CALI_TYPE_INT,
                                    CALI_ATTR_SCOPE_PROCESS |
                                    CALI_ATTR_ASVALUE       |
                                    CALI_ATTR_SKIP_EVENTS);

            sleep_sec =
                channel->config().init("monitor", s_configdata).get("interval").to_uint();
        }

public:

    static void create(Caliper* c, Channel* channel) {
        ThreadMonitor* instance = new ThreadMonitor(c, channel);

        channel->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->post_init_cb();
            });
        channel->events().finish_evt.connect(
            [instance](Caliper*, Channel*){
                instance->finish_cb();
                delete instance;
            });

        Log(1).stream() << channel->name()
                        << ": Registered thread_monitor service"
                        << std::endl;
    }

};

const ConfigSet::Entry ThreadMonitor::s_configdata[] = {
    { "interval", CALI_TYPE_INT, "2",
      "Monitor snapshot interval in seconds.",
      "Monitor snapshot interval in seconds."
    },
    ConfigSet::Terminator
};

} // namespace [anonymous]

namespace cali
{

CaliperService thread_monitor_service { "thread_monitor", ::ThreadMonitor::create };

}

// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Sampler.cpp
// Caliper sampler service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>

#include <unistd.h>

#include <sys/syscall.h>
#include <sys/types.h>

#include "context.h"

using namespace cali;
using namespace std;

namespace
{

Attribute   timer_attr   { Attribute::invalid };
Attribute   sampler_attr { Attribute::invalid };

cali_id_t   sampler_attr_id     = CALI_INV_ID;
int         nsec_interval       = 0;

int         n_samples           = 0;
int         n_processed_samples = 0;

Channel* channel          = nullptr;

static const ConfigSet::Entry s_configdata[] = {
    { "frequency", CALI_TYPE_INT, "50",
      "Sampling frequency (in Hz)",
      "Sampling frequency (in Hz)"
    },
    ConfigSet::Terminator
};

void on_prof(int sig, siginfo_t *info, void *context)
{
    ++n_samples;

    Caliper c = Caliper::sigsafe_instance();

    if (!c)
        return;

#ifdef CALI_SAMPLER_GET_PC
    uint64_t  pc = static_cast<uint64_t>( CALI_SAMPLER_GET_PC(context) );
    Variant v_pc(CALI_TYPE_ADDR, &pc, sizeof(uint64_t));

    SnapshotRecord trigger_info(1, &sampler_attr_id, &v_pc);
#else
    SnapshotRecord trigger_info;
#endif

    c.push_snapshot(channel, &trigger_info);

    ++n_processed_samples;
}

void setup_signal()
{
    sigset_t sigset;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGPROF);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);

    struct sigaction act;

    memset(&act, 0, sizeof(act));

    act.sa_sigaction = on_prof;
    act.sa_flags     = SA_RESTART | SA_SIGINFO;

    sigaction(SIGPROF, &act, NULL);
}

void clear_signal()
{
    signal(SIGPROF, SIG_IGN);
}

struct TimerWrap
{
    timer_t timer;
};

void setup_settimer(Caliper* c)
{
    struct sigevent sev;

    std::memset(&sev, 0, sizeof(sev));

    sev.sigev_notify   = SIGEV_THREAD_ID;     // Linux-specific!
    sev._sigev_un._tid = syscall(SYS_gettid);
    sev.sigev_signo    = SIGPROF;

    TimerWrap* twrap = new TimerWrap;

    if (timer_create(CLOCK_MONOTONIC, &sev, &twrap->timer) == -1) {
        Log(0).stream() << "sampler: timer_create() failed" << std::endl;
        return;
    }

    struct itimerspec spec;

    spec.it_interval.tv_sec  = 0;
    spec.it_interval.tv_nsec = nsec_interval;
    spec.it_value.tv_sec     = 0;
    spec.it_value.tv_nsec    = nsec_interval;

    if (timer_settime(twrap->timer, 0, &spec, NULL) == -1) {
        Log(0).stream() << "Sampler: timer_settime() failed" << std::endl;
        return;
    }

    Variant v_timer(cali_make_variant_from_ptr(twrap));
    c->set(timer_attr, v_timer);

    Log(2).stream() << "Sampler: Registered timer " << v_timer << endl;
}

void clear_timer(Caliper* c, Channel* chn) {
    Entry e = c->get(timer_attr);

    if (e.is_empty()) {
        Log(0).stream() << chn->name() << ": Sampler: Timer attribute not found " << endl;
        return;
    }

    TimerWrap* twrap = static_cast<TimerWrap*>(e.value().get_ptr());

    if (!twrap)
        return;

    Log(2).stream() << chn->name() << ": Sampler: Deleting timer " << e.value() << endl;

    timer_delete(twrap->timer);
    delete twrap;
}

void create_thread_cb(Caliper* c, Channel* chn) {
    setup_settimer(c);
}

void release_thread_cb(Caliper* c, Channel* chn) {
    clear_timer(c, chn);
}

void finish_cb(Caliper* c, Channel* chn) {
    clear_timer(c, chn);
    clear_signal();

    Log(1).stream() << chn->name()
                    << ": Sampler: processed " << n_processed_samples << " samples ("
                    << n_samples << " total, "
                    << n_samples - n_processed_samples << " dropped)." << endl;

    n_samples = 0;
    n_processed_samples = 0;

    channel = nullptr;
}

void sampler_register(Caliper* c, Channel* chn)
{
    if (channel) {
        Log(0).stream() << chn->name() << ": Sampler: Cannot enable sampler service twice!"
                        << " It is already enabled in channel "
                        << channel->name() << std::endl;

        return;
    }

    ConfigSet config = chn->config().init("sampler", s_configdata);

    Attribute symbol_class_attr = c->get_attribute("class.symboladdress");
    Variant v_true(true);

    timer_attr =
        c->create_attribute(std::string("cali.sampler.timer.")+std::to_string(chn->id()), CALI_TYPE_PTR,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_SKIP_EVENTS  |
                            CALI_ATTR_ASVALUE      |
                            CALI_ATTR_HIDDEN);
    sampler_attr =
        c->create_attribute("cali.sampler.pc", CALI_TYPE_ADDR,
                            CALI_ATTR_SCOPE_THREAD |
                            CALI_ATTR_SKIP_EVENTS  |
                            CALI_ATTR_ASVALUE,
                            1, &symbol_class_attr, &v_true);

    sampler_attr_id = sampler_attr.id();

    int frequency = config.get("frequency").to_int();

    // some sanity checking
    frequency     = std::min(std::max(frequency, 1), 10000);
    nsec_interval = 1000000000 / frequency;

    c->set(chn, c->create_attribute("sample.frequency", CALI_TYPE_INT, CALI_ATTR_GLOBAL),
           Variant(frequency));

    chn->events().create_thread_evt.connect(create_thread_cb);
    chn->events().release_thread_evt.connect(release_thread_cb);
    chn->events().finish_evt.connect(finish_cb);

    channel = chn;

    setup_signal();
    setup_settimer(c);

    Log(1).stream() << chn->name() << ": Registered sampler service. Using "
                    << frequency << "Hz sampling frequency." << endl;
}

} // namespace

namespace cali
{

CaliperService sampler_service { "sampler", ::sampler_register };

}

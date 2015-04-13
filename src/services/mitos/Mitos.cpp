///@file  Mitos.cpp
///@brief Mitos provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>
#include <SigsafeRWLock.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <pthread.h>
#include <omp.h>

using namespace cali;
using namespace std;

#include <Mitos.h>

namespace 
{

Attribute address_attr        { Attribute::invalid } ;

ConfigSet config;

bool      record_address { false };

static const ConfigSet::Entry s_configdata[] = {
    { "mitos", CALI_TYPE_BOOL, "false",
      "Include sampled load addresses",
      "Include sampled load addresses"
    },
    ConfigSet::Terminator
};

static pthread_key_t key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

void make_sample_key()
{
    pthread_key_create(&key, NULL);
    pthread_setspecific(key, NULL); 
}

void sample_handler(perf_event_sample *sample, void *args) {
    if (SigsafeRWLock::is_thread_locked())
        return;

    // copy sample to thread-specific data
    perf_event_sample *smp = new perf_event_sample;
    memcpy(smp,sample,sizeof(perf_event_sample));
    pthread_setspecific(key,smp);

    Caliper *c = (Caliper*)args;
    c->push_context(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS);
}

void push_load_sample(Caliper* c, int scope, WriteRecordFn fn) {
    Variant v_attr[1];
    Variant v_data[1];

    // get sample from thread-specific key
    perf_event_sample *sample = (perf_event_sample*)pthread_getspecific(key);
    if(!sample)
        return;

    v_attr[0] = address_attr.id();
    v_data[0] = sample->addr;

    int               n[3] = { 0,       1,  1  };
    const Variant* data[3] = { nullptr, v_attr, v_data };

    fn(ContextRecord::record_descriptor(), n, data);
}

void mitos_init(Caliper* c) {
    Mitos_set_sample_threshold(7);
    Mitos_set_sample_period(1000);
    Mitos_set_sample_mode(SMPL_MEMORY);
    Mitos_set_handler_fn(&sample_handler,c);
    Mitos_prepare(0);
    Mitos_begin_sampler();
}

void mitos_finish(Caliper* c)
{
    Mitos_end_sampler();
}

/// Initialization handler
void mitos_register(Caliper* c)
{
    record_address = config.get("address").to_bool();

    address_attr = 
        c->create_attribute("mitos.address", CALI_TYPE_ADDR, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_SKIP_EVENTS);

    // add callback for Caliper::get_context() event
    c->events().measure.connect(&push_load_sample);
    c->events().finish_evt.connect(&mitos_finish);

    // create thread-specific key
    pthread_once(&key_once, make_sample_key);

    // initialize mitos
    mitos_init(c);

    Log(1).stream() << "Registered mitos service" << endl;
}

} // namespace


namespace cali
{
    CaliperService MitosService = { "mitos", { ::mitos_register } };
} // namespace cali

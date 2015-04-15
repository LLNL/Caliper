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

void make_sample_key() {
    pthread_key_create(&key, NULL);
}

void sample_handler(perf_event_sample *sample, void *args) {
    if (SigsafeRWLock::is_thread_locked())
        return;
    
    perf_event_sample *smp = (perf_event_sample*)pthread_getspecific(key);

    if(!smp)
        return;

    memcpy(smp,sample,sizeof(perf_event_sample));

    Caliper *c = (Caliper*)args;
    c->push_context(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS);
}

void push_load_sample(Caliper* c, int scope, WriteRecordFn fn) {
    perf_event_sample *sample = (perf_event_sample*)pthread_getspecific(key);

    if(!sample)
    {
        //std::cerr << "NULL from key!\n";
        return;
    }
    if(sample->addr == 0)
    {
        //std::cerr << "Not set!\n";
        return;
    }

    Variant v_attr[1];
    Variant v_data[1];

    v_attr[0] = address_attr.id();
    v_data[0] = sample->addr;

    int               n[3] = { 0,       1,  1  };
    const Variant* data[3] = { nullptr, v_attr, v_data };

    fn(ContextRecord::record_descriptor(), n, data);
}

void thread_data_init(cali_context_scope_t cscope, ContextBuffer* cbuf) {
    // check if allocated
    void *thread_data = pthread_getspecific(key); 
    if(thread_data)
        return;

    // allocate sample
    perf_event_sample *sample = new perf_event_sample;
    memset(sample,0,sizeof(perf_event_sample));

    // point key
    pthread_setspecific(key,sample);
}

void mitos_init(Caliper* c) {
    Mitos_set_sample_threshold(3);
    Mitos_set_sample_period(1000);
    Mitos_set_sample_mode(SMPL_MEMORY);
    Mitos_set_handler_fn(&sample_handler,c);
    Mitos_prepare(0);
    Mitos_begin_sampler();
}

void mitos_finish(Caliper* c) {
    Mitos_end_sampler();
}

/// Initialization handler
void mitos_register(Caliper* c) {
    record_address = config.get("address").to_bool();

    address_attr = 
        c->create_attribute("mitos.address", CALI_TYPE_ADDR, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);

    // add callback for Caliper::get_context() event
    c->events().measure.connect(&push_load_sample);
    c->events().finish_evt.connect(&mitos_finish);
    c->events().create_context_evt.connect(&thread_data_init);

    // initialize master thread
    thread_data_init((cali_context_scope_t)0,NULL);

    // initialize per-thread data
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

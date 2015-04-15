///@file  Mitos.cpp
///@brief Mitos provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>
#include <ContextBuffer.h>
#include <SigsafeRWLock.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <unistd.h>
#include <sys/syscall.h>

#include <map>

using namespace cali;
using namespace std;

#include <Mitos.h>

pid_t gettid()
{
    return syscall(SYS_gettid);
}

namespace 
{

Attribute address_attr        { Attribute::invalid } ;

ConfigSet config;

bool      record_address { false };

perf_event_sample static_sample;

map<pid_t,ContextBuffer*> thread_context_map;
SigsafeRWLock thread_context_lock;

static const ConfigSet::Entry s_configdata[] = {
    { "mitos", CALI_TYPE_BOOL, "false",
      "Include sampled load addresses",
      "Include sampled load addresses"
    },
    ConfigSet::Terminator
};

static void sample_handler(perf_event_sample *sample, void *args) {
    // copy to global static sample
    memcpy(&static_sample,sample,sizeof(perf_event_sample));

    // push context to invoke push_sample
    Caliper *c = Caliper::instance();

    if(c == NULL)
    {
        std::cerr << "Null Caliper instance!\n";
    } else
        c->push_context(CALI_SCOPE_THREAD);
}

void push_sample(Caliper* c, int scope, WriteRecordFn fn) {
    // get context of sample thread
    thread_context_lock.rlock();
    auto find_context = thread_context_map.find(static_sample.tid);
    if(find_context == thread_context_map.end())
    {
        thread_context_lock.unlock();
        return; // not found
    }
    thread_context_lock.unlock();

    ContextBuffer *thread_context = find_context->second;

    Variant v_attr[1];
    Variant v_data[1];

    v_attr[0] = address_attr.id();
    v_data[0] = static_sample.addr;

    int               n[3] = { 0,       1,  1  };
    const Variant* data[3] = { nullptr, v_attr, v_data };

    fn(ContextRecord::record_descriptor(), n, data);

    thread_context->push_record(fn);
}

void mitos_init(Caliper* c) {
    Mitos_set_sample_threshold(3);
    Mitos_set_sample_period(1000);
    Mitos_set_sample_mode(SMPL_MEMORY);
    Mitos_set_handler_fn(&sample_handler,c);
    Mitos_prepare(0);
    Mitos_begin_sampler();

    Log(1).stream() << "Mitos sampling handler initialized" << endl;
}

void mitos_finish(Caliper* c) {
    Mitos_end_sampler();
}

void map_thread_context(cali_context_scope_t cscope,
                        ContextBuffer *cbuf)
{
    thread_context_lock.wlock();
    thread_context_map[gettid()] = cbuf;
    thread_context_lock.unlock();
}

/// Initialization handler
void mitos_register(Caliper* c) {
    record_address = config.get("address").to_bool();

    address_attr = 
        c->create_attribute("mitos.address", CALI_TYPE_ADDR, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);

    // add callback for Caliper::get_context() event
    c->events().post_init_evt.connect(&mitos_init);
    c->events().finish_evt.connect(&mitos_finish);
    c->events().create_context_evt.connect(&map_thread_context);
    c->events().measure.connect(&push_sample);

    // thread context map lock
    thread_context_lock.init();

    // map master thread's contextbuffer
    map_thread_context((cali_context_scope_t)0,c->current_contextbuffer(CALI_SCOPE_THREAD));

    Log(1).stream() << "Registered mitos service" << endl;
}

} // namespace


namespace cali 
{
    CaliperService MitosService = { "mitos", { ::mitos_register } };
} // namespace cali

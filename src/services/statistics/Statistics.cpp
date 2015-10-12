/// @file  Statistics.cpp
/// @brief Caliper statistics service

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>

#include <atomic>

using namespace cali;
using namespace std;

namespace
{

atomic<unsigned> num_attributes;
atomic<unsigned> num_snapshots;
atomic<unsigned> num_updates;
atomic<unsigned> num_contexts;

void create_attr_cb(Caliper*, const Attribute&)
{
    ++num_attributes;
}

void update_cb(Caliper*, const Attribute&)
{
    ++num_updates;
}

void create_context_cb(cali_context_scope_t, ContextBuffer*)
{
    ++num_contexts;
}

void snapshot_cb(Caliper*, int, const Caliper::Entry*, Snapshot*)
{
    ++num_snapshots;
}

void finish_cb(Caliper* c)
{
    Log(1).stream() << "Statistics:"
                    << "\n     Number of additional contexts: " << num_contexts.load() 
                    << "\n     Number of user attributes:     " << num_attributes.load()
                    << "\n     Number of context updates:     " << num_updates.load()
                    << "\n     Number of context snapshots:   " << num_snapshots.load() << endl;
}

void statistics_service_register(Caliper* c)
{
    num_attributes = 0;
    num_snapshots  = 0;
    num_updates    = 0;
    num_contexts   = 0;

    c->events().create_attr_evt.connect(&create_attr_cb);
    c->events().pre_begin_evt.connect(&update_cb);
    c->events().pre_end_evt.connect(&update_cb);
    c->events().pre_set_evt.connect(&update_cb);
    c->events().finish_evt.connect(&finish_cb);
    c->events().create_context_evt.connect(&create_context_cb);
    c->events().snapshot.connect(&snapshot_cb);

    Log(1).stream() << "Registered debug service" << endl;
}
    
} // namespace

namespace cali
{
    CaliperService StatisticsService = { "statistics", ::statistics_service_register };
}

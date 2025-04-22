// Copyright (c) 2015-2024, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// AsyncEventTrigger.cpp
// Caliper async event trigger

#include "../Services.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"

using namespace cali;

namespace
{

void async_event_trigger_register(Caliper*, Channel* channel)
{
    channel->events().async_event.connect([](Caliper* c, ChannelBody* chB, SnapshotView info){
            if (!info.empty())
                c->push_snapshot(chB, info);
        });

    Log(1).stream() << channel->name() << ": registered async_event service\n";
}

const char* s_spec =  R"json(
{
 "name": "async_event",
 "description": "Trigger snapshots for asynchronous events"
})json";

} // namespace [anonymous]

namespace cali
{

CaliperService async_event_service { ::s_spec, ::async_event_trigger_register };

}
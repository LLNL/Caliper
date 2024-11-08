// Copyright (c) 2015-2024, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/AsyncEvent.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

using namespace cali;

namespace
{

Attribute get_async_event_begin_attr(Caliper& c)
{
    static std::atomic<Node*> s_attr_node { nullptr };

    auto node = s_attr_node.load();
    if (node)
        return Attribute::make_attribute(node);

    Attribute attr = c.create_attribute("async.begin", CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
    s_attr_node.store(attr.node());

    return attr;
}

Attribute get_async_event_end_attr(Caliper& c)
{
    static std::atomic<Node*> s_attr_node { nullptr };

    auto node = s_attr_node.load();
    if (node)
        return Attribute::make_attribute(node);

    Attribute attr = c.create_attribute("async.end", CALI_TYPE_STRING, CALI_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
    s_attr_node.store(attr.node());

    return attr;
}

Attribute get_event_duration_attr(Caliper& c)
{
    static std::atomic<Node*> s_attr_node { nullptr };

    auto node = s_attr_node.load();
    if (node)
        return Attribute::make_attribute(node);

    Attribute attr = c.create_attribute("event.duration.ns", CALI_TYPE_UINT, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE | CALI_ATTR_SKIP_EVENTS);
    s_attr_node.store(attr.node());

    return attr;
}

Node async_event_root_node { CALI_INV_ID, CALI_INV_ID, Variant() };

}

void TimedAsyncEvent::end()
{
    uint64_t nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start_time_).count();

    Caliper c;
    Attribute duration_attr = ::get_event_duration_attr(c);
    const Entry data[2] = {
        { end_tree_node_ }, { duration_attr, cali_make_variant_from_uint(nsec) }
    };

    c.async_event(SnapshotView(2, data));
}

TimedAsyncEvent TimedAsyncEvent::begin(const char* message)
{
    Caliper c;
    Attribute begin_attr = ::get_async_event_begin_attr(c);
    Attribute end_attr = ::get_async_event_end_attr(c);

    Variant v_msg(message);
    Node* begin_node = c.make_tree_entry(begin_attr, v_msg, &::async_event_root_node);
    Node* end_node = c.make_tree_entry(end_attr, v_msg, &::async_event_root_node);

    c.async_event(SnapshotView(Entry(begin_node)));

    return TimedAsyncEvent(end_node);
}
// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// AnnotationBinding.cpp
// Base class for implementing Caliper-to-X annotation bindings

#include "caliper/AnnotationBinding.h"

#include "RegionFilter.h"

#include "caliper/common/Node.h"
#include "caliper/common/Log.h"

using namespace cali;

//
// --- Static data
//

const cali::ConfigSet::Entry AnnotationBinding::s_configdata[] = {
    { "include_regions", CALI_TYPE_STRING, "",
      "Region filter specifying regions to include",
      "Region filter specifying regions to include",
    },
    { "exclude_regions", CALI_TYPE_STRING, "",
      "Region filter specifying regions to exclude",
      "Region filter specifying regions to exclude",
    },
    { "trigger_attributes", CALI_TYPE_STRING, "",
      "List of attributes that trigger the annotation binding",
      "List of attributes that trigger the annotation binding"
    },
    cali::ConfigSet::Terminator
};

namespace cali
{
    extern Attribute subscription_event_attr;
}

namespace
{

bool has_marker(const Attribute& attr, const Attribute& marker_attr)
{
    cali_id_t marker_attr_id = marker_attr.id();

    for (const Node* node = attr.node()->first_child(); node; node = node->next_sibling())
        if (node->attribute() == marker_attr_id)
            return true;

    return false;
}

}

//
// --- Implementation
//

bool
AnnotationBinding::is_subscription_attribute(const Attribute& attr)
{
    return attr.get(subscription_event_attr).to_bool();
}

AnnotationBinding::~AnnotationBinding()
{ }

void
AnnotationBinding::mark_attribute(Caliper *c, Channel* chn, const Attribute& attr)
{
    // Add the binding marker for this attribute

    Variant v_true(true);
    c->make_tree_entry(m_marker_attr, v_true, attr.node());

    // Invoke derived functions

    on_mark_attribute(c, chn, attr);

    Log(2).stream() << "Adding " << this->service_tag()
                    << " bindings for attribute \"" << attr.name()
                    << "\" in " << chn->name() << " channel" << std::endl;
}

void
AnnotationBinding::check_attribute(Caliper* c, Channel* chn, const Attribute& attr)
{
    int prop = attr.properties();

    if (prop & CALI_ATTR_SKIP_EVENTS)
        return;

    if (m_trigger_attr_names.empty()) {
        // By default, enable binding only for class.nested attributes
        if (!(prop & CALI_ATTR_NESTED))
            return;
    } else {
        if (std::find(m_trigger_attr_names.begin(), m_trigger_attr_names.end(),
                      attr.name()) == m_trigger_attr_names.end())
            return;
    }

    mark_attribute(c, chn, attr);
}

void
AnnotationBinding::begin_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)
{
    if (!::has_marker(attr, m_marker_attr))
        return;
    if (m_filter && !m_filter->pass(value))
        return;

    this->on_begin(c, chn, attr, value);
}

void
AnnotationBinding::end_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)
{
    if (!::has_marker(attr, m_marker_attr))
        return;
    if (m_filter && !m_filter->pass(value))
        return;

    this->on_end(c, chn, attr, value);
}

void
AnnotationBinding::base_pre_initialize(Caliper* c, Channel* chn)
{
    const char* tag = service_tag();
    std::string cfgname = std::string(tag) + "_binding";

    m_config = chn->config().init(cfgname.c_str(), s_configdata);

    {
        std::string i_filter =
            m_config.get("include_regions").to_string();
        std::string e_filter =
            m_config.get("exclude_regions").to_string();

        auto p = RegionFilter::from_config(i_filter, e_filter);

        if (!p.second.empty()) {
            Log(0).stream() << chn->name() << ": event: filter parse error: "
                            << p.second
                            << std::endl;
        } else if (p.first.has_filters()) {
            m_filter = std::unique_ptr<RegionFilter>(new RegionFilter(std::move(p.first)));
        }
    }

    m_trigger_attr_names =
        m_config.get("trigger_attributes").to_stringlist(",:");

    std::string marker_attr_name("cali.binding.");
    marker_attr_name.append(tag);
    marker_attr_name.append("#");
    marker_attr_name.append(std::to_string(chn->id()));

    m_marker_attr =
        c->create_attribute(marker_attr_name, CALI_TYPE_BOOL,
                            CALI_ATTR_HIDDEN | CALI_ATTR_SKIP_EVENTS);
}

void
AnnotationBinding::base_post_initialize(Caliper* c, Channel* chn)
{
    // check and mark existing attributes

    std::vector<Attribute> attributes = c->get_all_attributes();

    for (const Attribute& attr : attributes)
        if (!attr.skip_events() && !is_subscription_attribute(attr))
            check_attribute(c, chn, attr);
}

AnnotationBinding::AnnotationBinding()
{ }
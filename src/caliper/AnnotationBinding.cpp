// Copyright (c) 2015-2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// AnnotationBinding.cpp
// Base class for implementing Caliper-to-X annotation bindings

#include "caliper/AnnotationBinding.h"

#include "caliper/common/Node.h"
#include "caliper/common/Log.h"

#include "caliper/common/filters/RegexFilter.h"

using namespace cali;

//
// --- Static data
//

const cali::ConfigSet::Entry AnnotationBinding::s_configdata[] = {
    { "regex_filter", CALI_TYPE_STRING, "",
      "Regular expression for matching annotations",
      "Regular expression for matching annotations"
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
{
    delete m_filter;
}

void
AnnotationBinding::mark_attribute(Caliper *c, Channel* chn, const Attribute& attr)
{
    // Add the binding marker for this attribute

    Variant v_true(true);
    c->make_tree_entry(m_marker_attr, v_true, c->node(attr.node()->id()));

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
    if (m_filter && !m_filter->filter(attr, value))
        return;

    this->on_begin(c, chn, attr, value);
}

void
AnnotationBinding::end_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)
{
    if (!::has_marker(attr, m_marker_attr))
        return;
    if (m_filter && !m_filter->filter(attr, value))
        return;

    this->on_end(c, chn, attr, value);
}

void
AnnotationBinding::base_pre_initialize(Caliper* c, Channel* chn)
{
    const char* tag = service_tag();
    std::string cfgname = std::string(tag) + "_binding";

    m_config = chn->config().init(cfgname.c_str(), s_configdata);

    if (m_config.get("regex").to_string().size() > 0)
        m_filter = new RegexFilter(tag, m_config);

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

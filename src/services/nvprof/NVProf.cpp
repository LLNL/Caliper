// Copyright (c) 2015, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Caliper NVidia profiler annotation binding

#include "caliper/AnnotationBinding.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include <nvToolsExt.h>

#include <atomic>
#include <cassert>
#include <map>
#include <mutex>
#include <unordered_map>

namespace cali
{

class NVProfBinding : public AnnotationBinding
{
    static const ConfigSet::Entry s_configdata[];

    static const uint32_t s_colors[];
    static const int      s_num_colors = 14;

    Attribute             m_color_attr;
    std::atomic<int>      m_color_id;

    std::map<cali_id_t, nvtxDomainHandle_t> m_domain_map;
    std::mutex            m_domain_mutex;

    bool                  m_cycle_colors;

    std::unordered_map<std::string,uint32_t> m_color_map;
    std::mutex            m_color_map_mutex;

    uint32_t get_attribute_color(const Attribute& attr) {
        cali_id_t color_attr_id = m_color_attr.id();
        const Node* node = nullptr;

        for (node = attr.node()->first_child(); node; node = node->next_sibling())
            if (node->attribute() == color_attr_id)
                break;

        return node ? static_cast<uint32_t>(node->data().to_uint()) : s_colors[0];
    }

    uint32_t get_value_color(const Variant& value) {
        std::string valstr = value.to_string();
        uint32_t color = s_colors[0];

        {
            std::lock_guard<std::mutex>
                g(m_color_map_mutex);

            auto it = m_color_map.find(valstr);

            if (it == m_color_map.end()) {
                color = s_colors[m_color_id++ % s_num_colors];
                m_color_map.emplace(std::make_pair(std::move(valstr), color));
            } else
                color = it->second;
        }

        return color;
    }

    uint32_t get_color(const Attribute& attr, const Variant& value) {
        if (m_cycle_colors)
            return get_value_color(value);
        else
            return get_attribute_color(attr);
    }

public:

    void initialize(Caliper* c, Channel* chn) {
        std::string name = "nvtx.color#";
        name.append(std::to_string(chn->id()));

        m_color_attr =
            c->create_attribute("nvtx.color", CALI_TYPE_UINT,
                                CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN);

        m_cycle_colors =
            chn->config().init("nvprof", s_configdata).get("cycle_colors").to_bool();
    }

    const char* service_tag() const { return "nvprof"; }

    void on_mark_attribute(Caliper* c, Channel*, const Attribute& attr) {
        if (m_cycle_colors)
            return;

        //   When cycle_colors is on, we cycle colors for each value.
        // Otherwise, we cycle colors for attributes, and mark the attribute
        // color here.

        // Set the color flag
        Variant v_color(static_cast<uint64_t>(s_colors[m_color_id++ % s_num_colors]));

        assert(m_color_attr != Attribute::invalid);

        c->make_tree_entry(m_color_attr, v_color, c->node(attr.node()->id()));
    }

    void on_begin(Caliper*, Channel*, const Attribute &attr, const Variant& value) {
        nvtxEventAttributes_t eventAttrib = { 0 };

        eventAttrib.version       = NVTX_VERSION;
        eventAttrib.size          = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
        eventAttrib.colorType     = NVTX_COLOR_ARGB;
        eventAttrib.color         = get_color(attr, value);
        eventAttrib.messageType   = NVTX_MESSAGE_TYPE_ASCII;
        eventAttrib.message.ascii =
            (value.type() == CALI_TYPE_STRING ? static_cast<const char*>(value.data()) : value.to_string().c_str());

        // For properly nested attributes, just use default push/pop.
        // For other attributes, create a domain.

        if (attr.is_nested()) {
            nvtxRangePushEx(&eventAttrib);
        } else {
            nvtxDomainHandle_t domain;

            {
                std::lock_guard<std::mutex>
                    g(m_domain_mutex);

                auto it = m_domain_map.find(attr.id());

                if (it == m_domain_map.end()) {
                    domain = nvtxDomainCreateA(attr.name().c_str());
                    m_domain_map.insert(std::make_pair(attr.id(), domain));
                } else {
                    domain = it->second;
                }
            }

            nvtxDomainRangePushEx(domain, &eventAttrib);
        }
    }

    void on_end(Caliper*, Channel*, const Attribute& attr, const Variant& value) {
        if (attr.is_nested()) {
            nvtxRangePop();
        } else {
            nvtxDomainHandle_t domain;

            {
                std::lock_guard<std::mutex>
                    g(m_domain_mutex);

                auto it = m_domain_map.find(attr.id());

                if (it == m_domain_map.end()) {
                    Log(0).stream() << "nvprof: on_end(): error: domain for attribute "
                                    << attr.name()
                                    << " not found!" << std::endl;
                    return;
                }

                domain = it->second;
            }

            nvtxDomainRangePop(domain);
        }
    }
};

const uint32_t NVProfBinding::s_colors[] = {
    0x0000cc00, 0x000000cc, 0x00cccc00, 0x00cc00cc,
    0x0000cccc, 0x00cc0000, 0x00cccccc,
    0x00008800, 0x00000088, 0x00888800, 0x00880088,
    0x00008888, 0x00880000, 0x00888888
};


const ConfigSet::Entry NVProfBinding::s_configdata[] = {
    { "cycle_colors", CALI_TYPE_BOOL, "true",
      "Use a different color for each annotation entry",
      "Use a different color for each annotation entry"
    },
    ConfigSet::Terminator
};

CaliperService nvprof_service { "nvprof", &AnnotationBinding::make_binding<NVProfBinding> };

} // namespace cali

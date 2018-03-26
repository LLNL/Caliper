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

#include <nvToolsExt.h>

#include <atomic>
#include <cassert>
#include <map>
#include <mutex>

namespace cali
{

class NVProfBinding : public AnnotationBinding
{
    static const uint32_t s_colors[];
    static const int      s_num_colors = 7;

    Attribute             m_color_attr;
    std::atomic<int>      m_color_id;

    std::map<cali_id_t, nvtxDomainHandle_t> m_domain_map;
    std::mutex            m_domain_mutex;

public:

    void initialize(Caliper* c) {
        m_color_attr =
            c->create_attribute("nvtx.color", CALI_TYPE_UINT,
                                CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN);
    }

    const char* service_tag() const { return "nvvp"; }

    void on_create_attribute(Caliper* c, const std::string& name, cali_attr_type, int*, Node** node) {
        // Set the color flag
        Variant v_color(static_cast<uint64_t>(s_colors[m_color_id++ % s_num_colors]));

        assert(m_color_attr != Attribute::invalid);

         *node = c->make_tree_entry(m_color_attr, v_color, *node);
    }

    void on_begin(Caliper*, const Attribute &attr, const Variant& value) {
        Variant  v_color = attr.get(m_color_attr);
        uint32_t color   =
            v_color.empty() ? s_colors[0] : static_cast<uint32_t>(v_color.to_uint());

        nvtxEventAttributes_t eventAttrib = { 0 };

        eventAttrib.version       = NVTX_VERSION;
        eventAttrib.size          = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
        eventAttrib.colorType     = NVTX_COLOR_ARGB;
        eventAttrib.color         = color;
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

    void on_end(Caliper*, const Attribute& attr, const Variant& value) {
        if (attr.is_nested()) {
            nvtxRangePop();
        } else {
            nvtxDomainHandle_t domain;

            {
                std::lock_guard<std::mutex>
                    g(m_domain_mutex);

                auto it = m_domain_map.find(attr.id());

                if (it == m_domain_map.end()) {
                    Log(0).stream() << "nvvp: on_end(): error: domain for attribute "
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
    0x0000cccc, 0x00cc0000, 0x00cccccc
};

CaliperService nvprof_service { "nvprof", &AnnotationBinding::make_binding<NVProfBinding> };

} // namespace cali

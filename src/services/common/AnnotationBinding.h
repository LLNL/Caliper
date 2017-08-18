// Copyright (c) 2015-2017, Lawrence Livermore National Security, LLC.  
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

/// \file  AnnotationBinding.h
/// \brief Base class for implementing Caliper-to-X annotation bindings

#pragma once

#include "../CaliperService.h"

#include "caliper/caliper-config.h"
#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <iostream>
#include <vector>

namespace cali 
{

class Filter;
class Node;

class AnnotationBinding 
{
    ConfigSet m_config;
    Filter*   m_filter;
    Attribute m_marker_attr;

    std::vector<std::string> m_trigger_attr_names;

    static const ConfigSet::Entry s_configdata[];

protected:

    void pre_create_attr_cb(Caliper*, const std::string&, cali_attr_type, int*, Node**);

    void begin_cb(Caliper*, const Attribute&, const Variant&);
    void end_cb(Caliper*, const Attribute&, const Variant&);

    void pre_initialize(Caliper* c);

    virtual void on_create_attribute(Caliper*,const std::string&, cali_attr_type, int*, Node**) { }

    virtual void on_begin(Caliper* c, const Attribute& attr, const Variant& value) { }
    virtual void on_end(Caliper* c, const Attribute& attr, const Variant& value)   { }

    virtual void initialize(Caliper* c) { }
    virtual void finalize(Caliper* c)   { }

public:

    AnnotationBinding()
        : m_filter(nullptr),
          m_marker_attr(Attribute::invalid)
        { }

    virtual ~AnnotationBinding();

    virtual const char* service_tag() const = 0;

    template <class BindingT>
    static void make_binding(Caliper* c) {
        BindingT* binding = new BindingT();
        binding->pre_initialize(c);
        binding->initialize(c);

        c->events().pre_create_attr_evt.connect(
            [binding](Caliper* c, const std::string& name, cali_attr_type type, int* prop, Node** node){
                binding->pre_create_attr_cb(c, name, type, prop, node);
            });
        c->events().pre_begin_evt.connect([binding](Caliper* c,const Attribute& attr,const Variant& value){
                binding->begin_cb(c,attr,value);
            });
        c->events().pre_end_evt.connect([binding](Caliper* c,const Attribute& attr,const Variant& value){
                binding->end_cb(c,attr,value);
            });
        c->events().finish_evt.connect([binding](Caliper* c){
                binding->finalize(c);
                delete binding;
            });

        Log(1).stream() << "Registered " << binding->service_tag() << " binding" << std::endl;
    }
};

} // namespace cali

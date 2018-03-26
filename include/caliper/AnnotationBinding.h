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

#include "caliper/CaliperService.h"

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

/// \brief Base class for bindings to third-party annotation APIs
///
/// This is a convenient base class for bindings to third-party annotation
/// APIs, i.e., mapping %Caliper regions to another tool's begin/end style
/// interface. To implement a mapping, create a derived class, overwrite
/// the \a on_begin() and \a on_end() functions, and initialize the mapping
/// during %Caliper initialization with \a make_binding().
///
/// By default, the \a on_begin() and \a on_end() methods will be invoked
/// only for properly nested annotations (i.e., attributes with the
/// CALI_ATTR_NESTED flag), or attributes selected at runtime with the
/// \a CALI_<tag_name>_ATTRIBUTES configuration variable.
///
/// An annotation binding must be created during %Caliper initialization
/// with the \a make_binding() template.
///
/// Example: Creating a service to bind %Caliper annotations to an
/// assumed \a mybegin(const char*), \a myend() API.
///
/// \code
///    class MyBinding : public cali::AnnotationBinding {
///      public:
///
///        const char* service_tag() const { 
///          return "mybinding"; 
///        }
///
///        void on_begin(cali::Caliper* c, const cali::Attribute& attr, const cali::Variant& value) {
///          std::string str = attr.name();
///          str.append("=");
///          str.append(value.to_string()); // create "<attribute name>=<value>" string
///          
///          mybegin(str.c_str());
///        }
///
///        void on_end(cali::Caliper* c, const cali::Attribute& attr, const cali::Variant& value) {
///          myend();
///        }
///   };
///
///   CaliperService mybinding_service { "mybinding", AnnotationBinding::make_binding<MyBinding> };
/// \endcode
///
/// Also see the \a nvprof and \a vtune service implementations for examples of
/// using AnnotationBinding in a %Caliper service.

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

    /// \brief Attribute creation callback
    ///
    /// This callback is invoked before a %Caliper attribute is being created 
    /// (only for properly nested or runtime-selected attributes).
    /// The callback allows additional metadata to be attached to the attribute
    /// by overwriting \a node.
    ///
    /// \param c    The Caliper instance
    /// \param name %Attribute name
    /// \param type %Attribute datatype
    /// \param prop %Attribute properties. Can be overwritten. Combination of
    ///   cali_attr_properties flags.
    /// \param node Context tree node under which the attribute
    ///   information will be attached. Can be overwritten, but should
    ///   retain the original node as parent.
    virtual void on_create_attribute(Caliper*           c,
                                     const std::string& name,
                                     cali_attr_type     type,
                                     int*               prop,
                                     Node**             node)
    { }

    /// \brief Callback for an annotation begin event
    /// \param c     Caliper instance
    /// \param attr  Attribute on which the %Caliper begin event was invoked.
    /// \param value The annotation name/value. 
    virtual void on_begin(Caliper* c, const Attribute& attr, const Variant& value) { }

    /// \brief Callback for an annotation end event
    /// \param c     Caliper instance
    /// \param attr  Attribute on which the %Caliper end event was invoked.
    /// \param value The annotation name/value. 
    virtual void on_end(Caliper* c, const Attribute& attr, const Variant& value)   { }

    /// \brief Initialization callback. Invoked after the %Caliper
    ///   initialization completed.
    virtual void initialize(Caliper* c) { }

    /// \brief Invoked on %Caliper finalization.
    virtual void finalize(Caliper* c)   { }

public:

    /// \brief Constructor. Usually invoked through \a make_binding().
    AnnotationBinding()
        : m_filter(nullptr),
          m_marker_attr(Attribute::invalid)
        { }

    virtual ~AnnotationBinding();

    /// \brief Name for the binding
    ///
    /// Name for the binding. This is used to derive the runtime configuration
    /// variable names. Must be overwritten. The name cannot contain spaces
    /// or special characters.
    virtual const char* service_tag() const = 0;

    /// \brief Create and setup a derived AnnotationBinding instance
    ///
    /// Creates the an instance of the binding and sets up all necessary
    /// %Caliper callback functions. Can be used as a %Caliper service
    /// initialization function.
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

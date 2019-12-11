// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

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
///        void on_begin(cali::Caliper* c, cali::Channel*, const cali::Attribute& attr, const cali::Variant& value) {
///          std::string str = attr.name();
///          str.append("=");
///          str.append(value.to_string()); // create "<attribute name>=<value>" string
///          
///          mybegin(str.c_str());
///        }
///
///        void on_end(cali::Caliper* c, cali::Channel*, const cali::Attribute& attr, const cali::Variant& value) {
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

    void mark_attribute(Caliper*, Channel*, const Attribute&);
    void check_attribute(Caliper*, Channel*, const Attribute&);

    void base_pre_initialize(Caliper*, Channel*);
    void base_post_initialize(Caliper*, Channel*);

    /// These callbacks are internal Caliper mechanisms
    /// not meant to be touched by the code of services
    /// implementing an AnnotationBinding.
    /// 
    /// User code should instead use on_mark_attribute(),
    /// on_begin(), and on_end().

    void begin_cb(Caliper*, Channel*, const Attribute&, const Variant&);
    void end_cb(Caliper*, Channel*, const Attribute&, const Variant&);

    static bool is_subscription_attribute(const Attribute& attr);

protected:

    /// \brief Attribute creation callback
    ///
    /// This callback is invoked when an attribute that should trigger this
    /// annotation binding is found.
    ///
    /// \param c    The %Caliper instance
    /// \param chn  The channel instance
    /// \param attr The attribute being marked
    virtual void on_mark_attribute(Caliper*           c,
                                   Channel*           chn,
                                   const Attribute&   attr)
    { }

    /// \brief Callback for an annotation begin event
    /// \param c     Caliper instance
    /// \param attr  Attribute on which the %Caliper begin event was invoked.
    /// \param value The annotation name/value. 
    virtual void on_begin(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) { }

    /// \brief Callback for an annotation end event
    /// \param c     Caliper instance
    /// \param attr  Attribute on which the %Caliper end event was invoked.
    /// \param value The annotation name/value. 
    virtual void on_end(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value)   { }

    /// \brief Initialization callback. Invoked after the %Caliper
    ///   initialization completed.
    virtual void initialize(Caliper* c, Channel* chn) { }

    /// \brief Invoked on %Caliper finalization.
    virtual void finalize(Caliper* c, Channel* chn)   { }

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
    static void make_binding(Caliper* c, Channel* chn) {
        BindingT* binding = new BindingT();
        binding->base_pre_initialize(c, chn);
        binding->initialize(c, chn);
        binding->base_post_initialize(c, chn);

        chn->events().create_attr_evt.connect(
            [binding](Caliper* c, Channel* chn, const Attribute& attr){
                if (!is_subscription_attribute(attr))
                    binding->check_attribute(c, chn, attr);
            });
        chn->events().subscribe_attribute.connect(
            [binding](Caliper* c, Channel* chn, const Attribute& attr){
                binding->check_attribute(c, chn, attr);
            });
        chn->events().pre_begin_evt.connect(
            [binding](Caliper* c, Channel* chn,const Attribute& attr,const Variant& value){
                binding->begin_cb(c,chn,attr,value);
            });
        chn->events().pre_end_evt.connect(
            [binding](Caliper* c, Channel* chn,const Attribute& attr,const Variant& value){
                binding->end_cb(c,chn,attr,value);
            });
        chn->events().finish_evt.connect(
            [binding](Caliper* c, Channel* chn){
                binding->finalize(c,chn);
                delete binding;
            });

        Log(1).stream() << "Registered " << binding->service_tag()
                        << " binding for channel " << chn->name() << std::endl;
    }
};

} // namespace cali

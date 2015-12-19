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

/// \file Annotation.cpp
/// Annotation interface

#include "Annotation.h"

#include "Caliper.h"

#include <Log.h>
#include <Variant.h>

#include <cstring>
#include <string>

using namespace std;
using namespace cali;


// --- Annotation implementation object

struct Annotation::Impl {
    Attribute   m_attr;
    std::string m_name;
    int         m_opt;
    int         m_refcount;

    Impl(const std::string& name, int opt)
        : m_attr(Attribute::invalid), 
          m_name(name), 
          m_opt(opt),
          m_refcount(1)
        { }

    void begin(const Variant& data) {
        Caliper c;
        
        if (m_attr == Attribute::invalid)
            create_attribute(c, data.type());

        if ((m_attr.type() == data.type()) && m_attr.type() != CALI_TYPE_INV)
            c.begin(m_attr, data);
    }

    void set(const Variant& data) {
        Caliper c;
                  
        if (m_attr == Attribute::invalid)
            create_attribute(c, data.type());

        if ((m_attr.type() == data.type()) && m_attr.type() != CALI_TYPE_INV)
            c.set(m_attr, data);
    }

    void end() {
        Caliper().end(m_attr);
    }

    void create_attribute(Caliper& c, cali_attr_type type) {
        m_attr = c.create_attribute(m_name, type, m_opt);

        if (m_attr == Attribute::invalid)
            Log(0).stream() << "Could not create attribute " << m_name << endl;
    }

    Impl* attach() {
        ++m_refcount;
        return this;
    }

    void detach() {
        --m_refcount;

        if (m_refcount == 0)
            delete this;
    }
};


// --- Guard subclass

Annotation::Guard::Guard(Annotation& a)
    : pI(a.pI->attach())
{ }

Annotation::Guard::~Guard()
{
    pI->end();
    pI->detach();
}


/// \class Annotation
///
/// \brief Instrumentation interface to add and manipulate context attributes
///
/// The Annotation class is the primary source-code instrumentation interface
/// for Caliper. Annotation objects provide access to named Caliper context 
/// attributes. If the referenced attribute does not exist yet, it will be 
/// created automatically.
///
/// Example:
/// \code
/// cali::Annotation phase_ann("myprogram.phase");
///
/// phase_ann.begin("Initialization");
///   // ...
/// phase_ann.end();
/// \endcode
/// This example creates an annotation object for the \c myprogram.phase 
/// attribute, and uses the \c begin()/end() methods to mark a section 
/// of code where that attribute is set to "Initialization".
///
/// Note that the access to a named context attribute through Annotation 
/// objects is not exclusive: two different Annotation objects can reference and
/// update the same context attribute.
///
/// The Annotation class is \em not threadsafe; however, threads can safely
/// access the same context attribute simultaneously through different 
/// Annotation objects.

// --- Constructors / destructor

/// Construct an annotation object for context attribute \c name. 
/// 

Annotation::Annotation(const char* name, int opt)
    : pI(new Impl(name, opt))
{ }

Annotation::~Annotation()
{
    pI->detach();
}


// --- begin() overloads

Annotation& Annotation::begin(int data)
{
    // special case: allow assignment of int values to 'double' or 'uint' attributes
    if (pI->m_attr.type() == CALI_TYPE_DOUBLE)
        return begin(Variant(static_cast<double>(data)));
    if (pI->m_attr.type() == CALI_TYPE_UINT)
        return begin(Variant(static_cast<uint64_t>(data)));

    return begin(Variant(data));
}

Annotation& Annotation::begin(double data)
{
    return begin(Variant(data));
}

Annotation& Annotation::begin(const char* data)
{
    return begin(Variant(CALI_TYPE_STRING, data, strlen(data)));
}

Annotation& Annotation::begin(cali_attr_type type, void* data, uint64_t size)
{
    return begin(Variant(type, data, size));
}

Annotation& Annotation::begin(const Variant& data)
{
    pI->begin(data);
    return *this;
}

// --- set() overloads

Annotation& Annotation::set(int data)
{
    // special case: allow assignment of int values to 'double' or 'uint' attributes
    if (pI->m_attr.type() == CALI_TYPE_DOUBLE)
        return set(Variant(static_cast<double>(data)));
    if (pI->m_attr.type() == CALI_TYPE_UINT)
        return set(Variant(static_cast<uint64_t>(data)));

    return set(Variant(data));
}

Annotation& Annotation::set(double data)
{
    return set(Variant(data));
}

Annotation& Annotation::set(const char* data)
{
    return set(Variant(CALI_TYPE_STRING, data, strlen(data)));
}

Annotation& Annotation::set(cali_attr_type type, void* data, uint64_t size)
{
    return set(Variant(type, data, size));
}

Annotation& Annotation::set(const Variant& data)
{
    pI->set(data);
    return *this;
}

void Annotation::end()
{
    pI->end();
}

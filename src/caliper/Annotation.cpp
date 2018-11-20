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

// Annotation interface

#include "caliper/Annotation.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"

#include <atomic>
#include <cstring>
#include <string>

using namespace std;
using namespace cali;

namespace cali
{

extern Attribute class_iteration_attr;
extern Attribute function_attr;
extern Attribute loop_attr;

}

// --- Pre-defined Function annotation class

Function::Function(const char* name)
{
    Caliper().begin(function_attr, Variant(CALI_TYPE_STRING, name, strlen(name)));
}

Function::~Function()
{
    Caliper().end(function_attr);
}
        
// --- Pre-defined loop annotation class

struct Loop::Impl {
    Attribute        iter_attr;
    std::atomic<int> level;
    std::atomic<int> refcount;
    
    Impl(const char* name)
        : level(0), refcount(1) {
        Variant v_true(true);
        
        iter_attr =
            Caliper().create_attribute(std::string("iteration#") + name,
                                       CALI_TYPE_INT,
                                       CALI_ATTR_ASVALUE,
                                       1, &class_iteration_attr, &v_true);
    }
};

Loop::Iteration::Iteration(const Impl* p, int i)
    : pI(p)
{
    Caliper().begin(p->iter_attr, Variant(i));
}

Loop::Iteration::~Iteration()
{
    Caliper().end(pI->iter_attr);
}

Loop::Loop(const char* name)
    : pI(new Impl(name))
{
    Caliper().begin(loop_attr, Variant(CALI_TYPE_STRING, name, strlen(name)));
    ++pI->level;
}

Loop::Loop(const Loop& loop)
    : pI(loop.pI)
{
    ++pI->refcount;
}

Loop::~Loop()
{
    if (--pI->refcount == 0) {
        end();
        delete pI;
    }
}

Loop::Iteration
Loop::iteration(int i) const
{
    return Iteration(pI, i);
}

void
Loop::end()
{
    if (pI->level > 0) {
        Caliper().end(loop_attr);
        --(pI->level);
    }
}

// --- Annotation implementation object

struct Annotation::Impl {
    std::atomic<Attribute*> m_attr;
    std::string             m_name;
    int                     m_opt;
    std::atomic<int>        m_refcount;

    Impl(const std::string& name, int opt)
        : m_attr(nullptr), 
          m_name(name), 
          m_opt(opt),
          m_refcount(1)
        { }

    ~Impl() {
        delete m_attr.load();
    }
    
    void begin(const Variant& data) {
        Caliper   c;
        Attribute attr = get_attribute(c, data.type());

        if ((attr.type() == data.type()) && attr.type() != CALI_TYPE_INV)
            c.begin(attr, data);
    }

    void set(const Variant& data) {
        Caliper   c;
        Attribute attr = get_attribute(c, data.type());

        if ((attr.type() == data.type()) && attr.type() != CALI_TYPE_INV)
            c.set(attr, data);
    }

    void end() {
        Caliper c;
        
        c.end(get_attribute(c));
    }

    Attribute get_attribute(Caliper& c, cali_attr_type type = CALI_TYPE_INV) {
        Attribute* attr_p = m_attr.load();

        if (!attr_p) {
            Attribute* new_attr = type == CALI_TYPE_INV ?
                new Attribute(c.get_attribute(m_name)) :
                new Attribute(c.create_attribute(m_name, type, m_opt));

            // Don't store invalid attribute in shared pointer
            if (*new_attr == Attribute::invalid)
                return Attribute::invalid;
            
            // Save new_attr iff m_attr == attr_p. If that is no longer the case,
            // some other thread has a set m_attr in the meantime, so just
            // delete our new object.
            if (!m_attr.compare_exchange_strong(attr_p, new_attr))
                delete new_attr;

            attr_p = m_attr.load();

            if (!attr_p || *attr_p == Attribute::invalid)
                Log(0).stream() << "Could not create attribute " << m_name << endl;
        }        

        return (attr_p ? *attr_p : Attribute::invalid);
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

// --- Constructors / destructor

Annotation::Annotation(const char* name, int opt)
    : pI(new Impl(name, opt))
{ }

Annotation::~Annotation()
{
    pI->detach();
}

Annotation::Annotation(const Annotation& a)
    : pI(a.pI->attach())
{ }

Annotation& Annotation::operator = (const Annotation& a)
{
    if (pI == a.pI)
        return *this;

    pI->detach();
    pI = a.pI->attach();

    return *this;
}


// --- begin() overloads

Annotation& Annotation::begin()
{
    return begin(Variant(true));
}

Annotation& Annotation::begin(int data)
{
    Attribute* attr = pI->m_attr.load();
    
    // special case: allow assignment of int values to 'double' or 'uint' attributes
    if (attr && attr->type() == CALI_TYPE_DOUBLE)
        return begin(Variant(static_cast<double>(data)));
    if (attr && attr->type() == CALI_TYPE_UINT)
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
    Attribute* attr = pI->m_attr.load();

    // special case: allow assignment of int values to 'double' or 'uint' attributes
    if (attr && attr->type() == CALI_TYPE_DOUBLE)
        return set(Variant(static_cast<double>(data)));
    if (attr && attr->type() == CALI_TYPE_UINT)
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

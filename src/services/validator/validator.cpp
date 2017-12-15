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

// validator.cpp
// Caliper annotation nesting validator

#include "../common/AnnotationBinding.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"

#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>

using namespace cali;


namespace
{

std::ostream& print_snapshot(Caliper* c, std::ostream& os) 
{
    SnapshotRecord::FixedSnapshotRecord<80> snapshot_data;
    SnapshotRecord snapshot(snapshot_data);

    c->pull_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS, nullptr, &snapshot);

    std::map< Attribute, std::vector<Variant> > rec = snapshot.unpack(*c);

    os << "{ ";

    int ca = 0;

    for ( auto const &p : rec ) {
        os << (ca++ > 0 ? ", " : "") << "\"" << p.first.name() << "\"=\"";

        int cv = 0;
        for ( auto const &v : p.second )
            os << (cv++ > 0 ? "/" : "") << v.to_string();

        os << "\"";
    }

    return os << " }";
}


Attribute class_nested_attr;

class StackValidator 
{
    std::map< Attribute, std::vector<Variant> > m_region_stack;
    bool m_error_found;

public:

    bool check_begin(const Attribute& attr, const Variant& value) {
        if (m_error_found)
            return true;

        m_region_stack[attr].push_back(value);

        if (attr.is_nested()) {
            cali_id_t   id = attr.id();
            Variant   v_id(CALI_TYPE_UINT, &id, sizeof(cali_id_t));

            m_region_stack[class_nested_attr].push_back(Variant(attr.id()));
        }

        return false;
    }

    bool check_end(Caliper* c, const Attribute& attr, const Variant& value) {
        if (m_error_found)
            return true;

        auto it = m_region_stack.find(attr);

        if (it == m_region_stack.end() || it->second.empty()) {
            // We currently can't actually check this situation because
            // the Caliper runtime prevents events being executed in
            // "empty stack" situations :-/

            m_error_found = true;

            print_snapshot(c,
                Log(0).stream() << "validator: end(\"" 
                                << attr.name() << "\"=\"" << value.to_string() << "\") "
                                << " has no matching begin().\n    context: " ) 
                << std::endl;
        } else {
            Variant v_stack_attr;

            if (attr.is_nested()) {
                auto n_it = m_region_stack.find(class_nested_attr);

                if (n_it != m_region_stack.end() && !n_it->second.empty()) {
                    v_stack_attr = n_it->second.back();
                    n_it->second.pop_back();
                }
            }

            Variant v_stack_val = it->second.back();
            it->second.pop_back();

            if (attr.is_nested() && attr.id() != v_stack_attr.to_id()) {
                m_error_found = true;

                print_snapshot(c,
                               Log(0).stream() << "validator: incorrect nesting: trying to end \""
                               << attr.name() << "\"=\"" << value.to_string() 
                               << "\" but current attribute is \"" 
                               << c->get_attribute(v_stack_attr.to_id()).name() 
                               << "\".\n    context: " )
                        << std::endl;
            } else if (!(value == v_stack_val)) {
                m_error_found = true;

                print_snapshot(c,
                    Log(0).stream() << "validator: incorrect nesting: trying to end \""
                                    << attr.name() << "\"=\"" << value.to_string() 
                                    << "\" but current value is \"" 
                                    << v_stack_val.to_string() << "\".\n    context: " )
                    << std::endl;
            }
        }

        return m_error_found;
    }

    bool check_final() {
        for (auto const &p : m_region_stack) {
            if (p.second.size() > 0) {
                std::ostringstream os;

                os << "validator: Regions not closed: "
                   << p.first.name() << "=";

                int cv = 0;
                for (auto const &v : p.second) 
                    os << (cv++ > 0 ? "/" : "") << v;

                Log(0).stream() << os.str() << std::endl;

                m_error_found = true;
            }
        }

        return m_error_found;
    }

    StackValidator()
        : m_error_found(false)
        { }
}; // class StackValidator


StackValidator*   proc_stack = nullptr;
std::mutex        proc_stack_mutex;

pthread_key_t     threadinfo_key;

std::atomic<int>  global_errors;

void destroy_thread_info(void* data)
{
    if (!data)
        return;

    StackValidator* v = static_cast<StackValidator*>(data);

    v->check_final();
    delete v;
}

StackValidator* get_thread_info()
{
    StackValidator* v = static_cast<StackValidator*>(pthread_getspecific(threadinfo_key));

    if (!v) {
        v = new StackValidator;

        pthread_setspecific(threadinfo_key, v);
    }

    return v;
}


void finalize_cb(Caliper*) 
{
    {
        std::lock_guard<std::mutex>
            g(proc_stack_mutex);

        if (proc_stack->check_final())
            ++global_errors;

        delete proc_stack;
        proc_stack = nullptr;
    }

    StackValidator* v = get_thread_info();

    if (v && v->check_final())
        ++global_errors;

    if (global_errors.load() > 0)
        Log(0).stream() << "validator: Annotation nesting errors found" 
                        << std::endl;
    else
        Log(1).stream() << "validator: No annotation nesting errors found" 
                        << std::endl;
}

void begin_cb(Caliper* c, const Attribute &attr, const Variant& value) 
{
    if ((attr.properties() & CALI_ATTR_SCOPE_MASK) == CALI_ATTR_SCOPE_PROCESS) {
        std::lock_guard<std::mutex>
            g(proc_stack_mutex);

        if (proc_stack->check_begin(attr, value))
            ++global_errors;
    } else {
        StackValidator* v = get_thread_info();

        if (v && v->check_begin(attr, value))
            ++global_errors;
    }
}

void end_cb(Caliper* c, const Attribute& attr, const Variant& value) 
{
    if ((attr.properties() & CALI_ATTR_SCOPE_MASK) == CALI_ATTR_SCOPE_PROCESS) {
        std::lock_guard<std::mutex>
            g(proc_stack_mutex);

        if (proc_stack->check_end(c, attr, value))
            ++global_errors;
    } else {
        StackValidator* v = get_thread_info();

        if (v && v->check_end(c, attr, value))
            ++global_errors;
    }
}

void validator_register(Caliper* c) 
{
    if (pthread_key_create(&threadinfo_key, destroy_thread_info) != 0) {
        Log(0).stream() << "validator: error: Could not create thread info" << std::endl;
        return;
    }

    class_nested_attr = 
        c->create_attribute("validator.nested", CALI_TYPE_UINT, CALI_ATTR_DEFAULT);

    proc_stack = new StackValidator;
    global_errors.store(0);

    c->events().finish_evt.connect(&finalize_cb);
    c->events().pre_begin_evt.connect(&begin_cb);
    c->events().pre_end_evt.connect(&end_cb);

    Log(1).stream() << "Registered validator service." << std::endl;
}

} // namespace [anonymous]


namespace cali
{
    CaliperService validator_service { "validator", ::validator_register };
}

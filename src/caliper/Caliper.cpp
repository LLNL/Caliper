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

/// \file Caliper.cpp
/// Caliper main class
///

#include "caliper-config.h"

#include "Caliper.h"
#include "ContextBuffer.h"
#include "MetadataTree.h"
#include "MemoryPool.h"
#include "SigsafeRWLock.h"
#include "Snapshot.h"

#include <Services.h>

#include <ContextRecord.h>
#include <Node.h>
#include <Log.h>
#include <RecordMap.h>
#include <RuntimeConfig.h>

#include <signal.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>
#include <utility>

using namespace cali;
using namespace std;


//
// --- Caliper implementation
//

struct Caliper::CaliperImpl
{
    // --- static data

    static volatile sig_atomic_t  s_siglock;
    static std::mutex             s_mutex;
    
    static unique_ptr<Caliper>    s_caliper;

    static const ConfigSet::Entry s_configdata[];

    // --- data

    ConfigSet              m_config;

    ContextBuffer*         m_default_process_context;
    ContextBuffer*         m_default_thread_context;
    ContextBuffer*         m_default_task_context;

    function<ContextBuffer*()>  m_get_thread_contextbuffer_cb;
    function<ContextBuffer*()>  m_get_task_contextbuffer_cb;
    
    MemoryPool             m_mempool;

    MetadataTree           m_tree;
    
    mutable SigsafeRWLock  m_attribute_lock;
    map<string, Node*>     m_attribute_nodes;

    Attribute              m_name_attr;
    Attribute              m_type_attr;
    Attribute              m_prop_attr;

    // Caliper version attribute
    Attribute              m_ver_attr;
    // Key attribute: one attribute stands in as key for all auto-merged attributes
    Attribute              m_key_attr;
    bool                   m_automerge;
    
    Events                 m_events;

    // --- constructor

    CaliperImpl()
        : m_config { RuntimeConfig::init("caliper", s_configdata) }, 
        m_default_process_context { new ContextBuffer },
        m_default_thread_context  { new ContextBuffer },
        m_default_task_context    { new ContextBuffer },
        m_name_attr { Attribute::invalid }, 
        m_type_attr { Attribute::invalid },  
        m_prop_attr { Attribute::invalid },
        m_ver_attr  { Attribute::invalid },
        m_key_attr  { Attribute::invalid },
        m_automerge { false }
    {
        m_automerge = m_config.get("automerge").to_bool();
    }

    ~CaliperImpl() {
        Log(1).stream() << "Finished" << endl;

        // prevent re-initialization
        s_siglock = 2;

	//freeing up context buffers
        delete m_default_process_context;
        delete m_default_thread_context; 
        delete m_default_task_context;
    }

    // deferred initialization: called when it's safe to use the public Caliper interface

    void 
    init() {
        bootstrap();
        
        Services::register_services(s_caliper.get());

        Log(1).stream() << "Initialized" << endl;

        m_events.post_init_evt(s_caliper.get());

        if (Log::verbosity() >= 2)
            RuntimeConfig::print( Log(2).stream() << "Configuration:\n" );
    }

    void  
    bootstrap() {                
        const MetaAttributeIDs* m = m_tree.meta_attribute_ids();
        
        m_name_attr = Attribute::make_attribute(m_tree.node(m->name_attr_id), m);
        m_type_attr = Attribute::make_attribute(m_tree.node(m->type_attr_id), m);
        m_prop_attr = Attribute::make_attribute(m_tree.node(m->prop_attr_id), m);

        m_key_attr  = create_attribute("cali.key.attribute", CALI_TYPE_USR, 0);

        assert(m_name_attr != Attribute::invalid);
        assert(m_type_attr != Attribute::invalid);
        assert(m_prop_attr != Attribute::invalid);
        assert(m_key_attr  != Attribute::invalid);
    }

    // --- helpers

    cali_context_scope_t 
    get_scope(const Attribute& attr) const {
        const struct scope_tbl_t {
            cali_attr_properties attrscope; cali_context_scope_t ctxscope;
        } scope_tbl[] = {
            { CALI_ATTR_SCOPE_THREAD,  CALI_SCOPE_THREAD  },
            { CALI_ATTR_SCOPE_PROCESS, CALI_SCOPE_PROCESS },
            { CALI_ATTR_SCOPE_TASK,    CALI_SCOPE_TASK    }
        };

        for (scope_tbl_t s : scope_tbl)
            if ((attr.properties() & CALI_ATTR_SCOPE_MASK) == s.attrscope)
                return s.ctxscope;

        // make thread scope the default
        return CALI_SCOPE_THREAD;
    }

    const Attribute&
    get_key(const Attribute& attr) const {
        if (!m_automerge || attr.store_as_value() || !attr.is_autocombineable())
            return attr;

        return m_key_attr;
    }


    // --- Environment interface

    ContextBuffer*
    default_contextbuffer(cali_context_scope_t scope) const {
        switch (scope) {
        case CALI_SCOPE_PROCESS:
            return m_default_process_context;
        case CALI_SCOPE_THREAD:
            return m_default_thread_context;
        case CALI_SCOPE_TASK:
            return m_default_task_context;
        }

        assert(!"Unknown context scope type!");

        return nullptr;
    }

    ContextBuffer*
    current_contextbuffer(cali_context_scope_t scope) const {
        switch (scope) {
        case CALI_SCOPE_PROCESS:
            return m_default_process_context;
        case CALI_SCOPE_THREAD:
            if (m_get_thread_contextbuffer_cb)
                return m_get_thread_contextbuffer_cb();
            break;
        case CALI_SCOPE_TASK:
            if (m_get_task_contextbuffer_cb)
                return m_get_task_contextbuffer_cb();
            break;
        }

        return default_contextbuffer(scope);
    }

    void 
    set_contextbuffer_callback(cali_context_scope_t scope, std::function<ContextBuffer*()> cb) {
        switch (scope) {
        case CALI_SCOPE_THREAD:
            if (m_get_thread_contextbuffer_cb)
                Log(0).stream() 
                    << "Caliper::set_context_callback(): error: callback already set" 
                    << endl;
            m_get_thread_contextbuffer_cb = cb;
            break;
        case CALI_SCOPE_TASK:
            if (m_get_task_contextbuffer_cb)
                Log(0).stream() 
                    << "Caliper::set_context_callback(): error: callback already set" 
                    << endl;
            m_get_task_contextbuffer_cb = cb;
            break;
        default:
            Log(0).stream() 
                << "Caliper::set_context_callback(): error: cannot set process callback" 
                << endl;
        }
    }

    ContextBuffer*
    create_contextbuffer(cali_context_scope_t scope) {
        ContextBuffer* ctx = new ContextBuffer;
        m_events.create_context_evt(scope, ctx);

        return ctx;
    }

    void
    release_contextbuffer(ContextBuffer* ctx) {
        m_events.destroy_context_evt(ctx);
        delete ctx;
    }

    // --- Attribute interface

    Attribute 
    create_attribute(const std::string& name, cali_attr_type type, int prop) {
        // Add default SCOPE_THREAD property if no other is set
        if (!(prop & CALI_ATTR_SCOPE_PROCESS) && !(prop & CALI_ATTR_SCOPE_TASK))
            prop |= CALI_ATTR_SCOPE_THREAD;

        Node* node { nullptr };

        // Check if an attribute with this name already exists

        m_attribute_lock.rlock();

        auto it = m_attribute_nodes.find(name);
        if (it != m_attribute_nodes.end())
            node = it->second;

        m_attribute_lock.unlock();

        // Create attribute nodes

        if (!node) {
            assert(type >= 0 && type <= CALI_MAXTYPE);
            Node* type_node = m_tree.type_node(type);
            assert(type_node);

            Attribute attr[2] { m_prop_attr, m_name_attr };
            Variant   data[2] { { prop }, { CALI_TYPE_STRING, name.c_str(), name.size() } };

            if (prop == CALI_ATTR_DEFAULT)
                node = m_tree.get_path(1, &attr[1], &data[1], type_node, &m_mempool);
            else
                node = m_tree.get_path(2, &attr[0], &data[0], type_node, &m_mempool);

            if (node) {
                // Check again if attribute already exists; might have been created by 
                // another thread in the meantime.
                // We've created some redundant nodes then, but that's fine
                m_attribute_lock.wlock();

                auto it = m_attribute_nodes.lower_bound(name);

                if (it == m_attribute_nodes.end() || it->first != name)
                    m_attribute_nodes.emplace_hint(it, name, node);
                else
                    node = it->second;

                m_attribute_lock.unlock();
            }
        }

        // Create attribute object

        Attribute attr = Attribute::make_attribute(node, m_tree.meta_attribute_ids());

        m_events.create_attr_evt(s_caliper.get(), attr);

        return attr;
    }

    Attribute
    get_attribute(const string& name) const {
        Node* node = nullptr;

        m_attribute_lock.rlock();

        auto it = m_attribute_nodes.find(name);

        if (it != m_attribute_nodes.end())
            node = it->second;

        m_attribute_lock.unlock();

        return Attribute::make_attribute(node, m_tree.meta_attribute_ids());
    }

    Attribute 
    get_attribute(cali_id_t id) const {
        return Attribute::make_attribute(m_tree.node(id), m_tree.meta_attribute_ids());
    }

    size_t
    num_attributes() const {
        m_attribute_lock.rlock();
        size_t size = m_attribute_nodes.size();
        m_attribute_lock.unlock();

        return size;
    }

    // --- Snapshot interface

    void
    pull_snapshot(int scope, const Entry* trigger_info, Snapshot* sbuf) {
        // Save trigger info in snapshot buf

        if (trigger_info) {
            Snapshot::Sizes     sizes = { 0, 0, 0 };
            Snapshot::Addresses addresses = sbuf->addresses();

            if (trigger_info->node()) {
                // node entry
                addresses.node_entries[sizes.n_nodes++]  = trigger_info->node();
            } else {
                // as-value entry
		// todo: what to do with hidden attribute? - trigger info shouldn't be hidden though
                addresses.immediate_attr[sizes.n_attr++] = trigger_info->attribute();
                addresses.immediate_data[sizes.n_data++] = trigger_info->value();
            }

            sbuf->commit(sizes);
        }

        // Invoke callbacks and get contextbuffer data

        m_events.snapshot(s_caliper.get(), scope, trigger_info, sbuf);

        for (cali_context_scope_t s : { CALI_SCOPE_TASK, CALI_SCOPE_THREAD, CALI_SCOPE_PROCESS })
            if (scope & s)
                current_contextbuffer(s)->snapshot(sbuf);
    }

    void 
    push_snapshot(int scope, const Entry* trigger_info) {
        // Create & pull snapshot

        Snapshot sbuf;

        pull_snapshot(scope, trigger_info, &sbuf);

        // Write any nodes that haven't been written 

        m_tree.write_new_nodes(m_events.write_record);        

        // Process

        m_events.process_snapshot(s_caliper.get(), trigger_info, &sbuf);
    }

    // --- Annotation interface

    cali_err 
    begin(const Attribute& attr, const Variant& data) {
        cali_err ret = CALI_EINV;

        if (attr == Attribute::invalid)
            return CALI_EINV;

        // invoke callbacks
        if (!attr.skip_events())
            m_events.pre_begin_evt(s_caliper.get(), attr);

        ContextBuffer* ctx = current_contextbuffer(get_scope(attr));

        if (attr.store_as_value())
            ret = ctx->set(attr, data);
        else
            ret = ctx->set_node(get_key(attr),
                                m_tree.get_path(1, &attr, &data, ctx->get_node(get_key(attr)), &m_mempool));

        // invoke callbacks
        if (!attr.skip_events())
            m_events.post_begin_evt(s_caliper.get(), attr);

        return ret;
    }

    cali_err 
    end(const Attribute& attr) {
        if (attr == Attribute::invalid)
            return CALI_EINV;

        cali_err ret = CALI_EINV;
        ContextBuffer* ctx = current_contextbuffer(get_scope(attr));

        // invoke callbacks
        if (!attr.skip_events())
            m_events.pre_end_evt(s_caliper.get(), attr);

        if (attr.store_as_value())
            ret = ctx->unset(attr);
        else {
            Node* node = ctx->get_node(get_key(attr));

            if (node) {
                node = m_tree.remove_first_in_path(node, attr, &m_mempool);
                
                if (node == m_tree.root())
                    ret = ctx->unset(get_key(attr));
                else if (node)
                    ret = ctx->set_node(get_key(attr), node);
            }

            if (!node)
                Log(0).stream() << "error: trying to end inactive attribute " << attr.name() << endl;
        }

        // invoke callbacks
        if (!attr.skip_events())
            m_events.post_end_evt(s_caliper.get(), attr);

        return ret;
    }

    cali_err 
    set(const Attribute& attr, const Variant& data) {
        cali_err ret = CALI_EINV;

        if (attr == Attribute::invalid)
            return CALI_EINV;

        ContextBuffer* ctx = current_contextbuffer(get_scope(attr));

        // invoke callbacks
        if (!attr.skip_events())
            m_events.pre_set_evt(s_caliper.get(), attr);

        if (attr.store_as_value())
            ret = ctx->set(attr, data);
        else
            ret = ctx->set_node(get_key(attr),
                                m_tree.replace_first_in_path(ctx->get_node(get_key(attr)), attr, data, &m_mempool));

        // invoke callbacks
        if (!attr.skip_events())
            m_events.post_set_evt(s_caliper.get(), attr);

        return ret;
    }


    cali_err 
    set_path(const Attribute& attr, size_t n, const Variant* data) {
        cali_err ret = CALI_EINV;

        if (attr == Attribute::invalid)
            return CALI_EINV;

        ContextBuffer* ctx = current_contextbuffer(get_scope(attr));

        // invoke callbacks
        if (!attr.skip_events())
            m_events.pre_set_evt(s_caliper.get(), attr);

        if (attr.store_as_value()) {
            Log(0).stream() << "error: set_path() invoked with immediate-value attribute " << attr.name() << endl;
            ret = CALI_EINV;
        } else
            ret = ctx->set_node(get_key(attr),
                                m_tree.replace_all_in_path(ctx->get_node(get_key(attr)), attr, n, data, &m_mempool));

        // invoke callbacks
        if (!attr.skip_events())
            m_events.post_set_evt(s_caliper.get(), attr);

        return ret;
    }

    // --- Query

    Entry
    get(const Attribute& attr) const {
        Entry e {  Entry::empty };

        if (attr == Attribute::invalid)
            return Entry::empty;

        ContextBuffer* ctx = current_contextbuffer(get_scope(attr));

        if (attr.store_as_value())
            return Entry(attr, ctx->get(attr));
        else
            return Entry(m_tree.find_node_with_attribute(attr, ctx->get_node(get_key(attr))));

        return e;
    }
};


// --- static member initialization

volatile sig_atomic_t  Caliper::CaliperImpl::s_siglock = 1;
mutex                  Caliper::CaliperImpl::s_mutex;

unique_ptr<Caliper>    Caliper::CaliperImpl::s_caliper;

const ConfigSet::Entry Caliper::CaliperImpl::s_configdata[] = {
    // key, type, value, short description, long description
    { "automerge", CALI_TYPE_BOOL, "true",
      "Automatically merge attributes into a common context tree", 
      "Automatically merge attributes into a common context tree.\n"
      "Decreases the size of context records, but may increase\n"
      "the amount of metadata and reduce performance." 
    },
    ConfigSet::Terminator 
};


//
// --- Caliper class definition
//

Caliper::Caliper()
    : mP(new CaliperImpl)
{ 
}

Caliper::~Caliper()
{
    mP.reset(nullptr);
}

// --- Generic entry API

Entry
Caliper::make_entry(size_t n, const Attribute* attr, const Variant* value) 
{
    // what do we do with as-value attributes here?!
    return Entry(mP->m_tree.get_path(n, attr, value, nullptr, &(mP->m_mempool)));
}

Entry 
Caliper::make_entry(const Attribute& attr, const Variant& value) 
{
    Entry entry { Entry::empty };

    if (attr.store_as_value())
        return Entry(attr, value);
    else
        return Entry(mP->m_tree.get_path(1, &attr, &value, nullptr, &(mP->m_mempool)));

    return entry;
}


// --- Events interface

Caliper::Events&
Caliper::events()
{
    return mP->m_events;
}


// --- Context environment API

ContextBuffer*
Caliper::default_contextbuffer(cali_context_scope_t scope) const
{
    return mP->default_contextbuffer(scope);
}

ContextBuffer*
Caliper::current_contextbuffer(cali_context_scope_t scope)
{
    return mP->current_contextbuffer(scope);
}

ContextBuffer*
Caliper::create_contextbuffer(cali_context_scope_t scope)
{
    return mP->create_contextbuffer(scope);
}

void
Caliper::release_contextbuffer(ContextBuffer* ctxbuf)
{
    mP->release_contextbuffer(ctxbuf);
}

void 
Caliper::set_contextbuffer_callback(cali_context_scope_t scope, std::function<ContextBuffer*()> cb)
{
    mP->set_contextbuffer_callback(scope, cb);
}

// --- Snapshot API

void 
Caliper::push_snapshot(int scopes, const Entry* trigger_info) 
{
    mP->push_snapshot(scopes, trigger_info);
}

void 
Caliper::pull_snapshot(int scopes, const Entry* trigger_info, Snapshot* sbuf) 
{
    mP->pull_snapshot(scopes, trigger_info, sbuf);
}

// --- Annotation interface

cali_err 
Caliper::begin(const Attribute& attr, const Variant& data)
{
    return mP->begin(attr, data);
}

cali_err 
Caliper::end(const Attribute& attr)
{
    return mP->end(attr);
}

cali_err 
Caliper::set(const Attribute& attr, const Variant& data)
{
    return mP->set(attr, data);
}

cali_err 
Caliper::set_path(const Attribute& attr, size_t n, const Variant* data)
{
    return mP->set_path(attr, n, data);
}

Variant
Caliper::exchange(const Attribute& attr, const Variant& data) {
    return mP->current_contextbuffer(mP->get_scope(attr))->exchange(attr, data);
}

// --- Attribute API

size_t
Caliper::num_attributes() const                       { return mP->num_attributes();    }
Attribute
Caliper::get_attribute(cali_id_t id) const            { return mP->get_attribute(id);   }
Attribute
Caliper::get_attribute(const std::string& name) const { return mP->get_attribute(name); }

Attribute 
Caliper::create_attribute(const std::string& name, cali_attr_type type, int prop)
{
    return mP->create_attribute(name, type, prop);
}

// --- Caliper query API

Entry
Caliper::get(const Attribute& attr) const {
    return mP->get(attr);
}

namespace
{
    // --- Exit handler

    void
    exit_handler(void) {
        Caliper* c = Caliper::instance();

        if (c)
            c->events().finish_evt(c);
    }
}

// --- Caliper singleton API

Caliper* Caliper::instance()
{
    if (CaliperImpl::s_siglock != 0) {
        if (CaliperImpl::s_siglock == 2)
            // Caliper had been initialized previously; we're past the static destructor
            return nullptr;

        if (atexit(::exit_handler) != 0)
            Log(0).stream() << "Unable to register exit handler";

        SigsafeRWLock::init();

        lock_guard<mutex> lock(CaliperImpl::s_mutex);

        if (!CaliperImpl::s_caliper) {
            CaliperImpl::s_caliper.reset(new Caliper);

            // now it is safe to use the Caliper interface
            CaliperImpl::s_caliper->mP->init();

            CaliperImpl::s_siglock = 0;
        }
    }

    return CaliperImpl::s_caliper.get();
}

Caliper* Caliper::try_instance()
{
    return CaliperImpl::s_siglock == 0 ? CaliperImpl::s_caliper.get() : nullptr;
}

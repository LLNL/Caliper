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
#include "EntryList.h"
#include "MetadataTree.h"
#include "MemoryPool.h"

#include <Services.h>

#include <ContextRecord.h>
#include <Node.h>
#include <Log.h>
#include <RuntimeConfig.h>

#include <signal.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <utility>

using namespace cali;
using namespace std;


namespace
{
    // --- helpers

    inline cali_context_scope_t 
    attr2caliscope(const Attribute& attr) {
        switch (attr.properties() & CALI_ATTR_SCOPE_MASK) {
        case CALI_ATTR_SCOPE_THREAD:
            return CALI_SCOPE_THREAD;
        case CALI_ATTR_SCOPE_PROCESS:
            return CALI_SCOPE_PROCESS;
        case CALI_ATTR_SCOPE_TASK:
            return CALI_SCOPE_TASK;    
        }

        // make thread scope the default
        return CALI_SCOPE_THREAD;
    }

    
    // --- Exit handler

    void
    exit_handler(void) {
        Caliper c = Caliper::instance();

        if (c) {
            c.events().flush(&c, nullptr);
            c.events().finish_evt(&c);

            c.release_scope(c.default_scope(CALI_SCOPE_PROCESS));
            // Somehow default thread scope is not released by pthread_key_create destructor
            c.release_scope(c.default_scope(CALI_SCOPE_THREAD));
        }

        // Don't delete global data, some thread-specific finalization may occur after this point 
        // Caliper::release();
    }

    // --- Siglock

    class siglock {
        volatile sig_atomic_t m_lock;

    public:

        siglock()
            : m_lock(0)
            { }

        inline void lock()   { ++m_lock; }
        inline void unlock() { --m_lock; }
        
        inline bool is_locked() const {
            return (m_lock > 0);
        }
    };
    
} // namespace


//
// Caliper Scope data
//

struct Caliper::Scope
{
    MemoryPool           mempool;
    ContextBuffer        blackboard;
    
    cali_context_scope_t scope;

    ::siglock            lock;

    Scope(cali_context_scope_t s)
        : scope(s) { }
};


//
// --- Caliper Global Data
//

struct Caliper::GlobalData
{
    // --- static data

    static volatile sig_atomic_t  s_init_lock;
    static std::mutex             s_init_mutex;
    
    static const ConfigSet::Entry s_configdata[];

    static GlobalData*            sG;
    
    // --- static functions

    static void release_thread(void* ctx) {
        Scope* scope = static_cast<Scope*>(ctx);
        
        Caliper(sG, scope, 0).release_scope(scope);
    }

    // --- data

    ConfigSet              config;

    ScopeCallbackFn        get_thread_scope_cb;
    ScopeCallbackFn        get_task_scope_cb;

    MetadataTree           tree;
    
    mutable std::mutex     attribute_lock;
    map<string, Node*>     attribute_nodes;

    // are there new attributes since last snapshot recording? - temporary, will go away
    std::atomic<bool>      new_attributes;
    std::atomic<bool>      bootstrap_nodes_written;

    Attribute              name_attr;
    Attribute              type_attr;
    Attribute              prop_attr;

    // Key attribute: one attribute stands in as key for all auto-merged attributes
    Attribute              key_attr;
    bool                   automerge;
    
    Events                 events;

    Scope*                 process_scope;
    Scope*                 default_thread_scope;
    Scope*                 default_task_scope;

    pthread_key_t          thread_scope_key;

    // --- constructor

    GlobalData()
        : config { RuntimeConfig::init("caliper", s_configdata) },
          get_thread_scope_cb { nullptr },
          get_task_scope_cb   { nullptr },
          new_attributes          { true  },
          bootstrap_nodes_written { false },
          name_attr { Attribute::invalid }, 
          type_attr { Attribute::invalid },  
          prop_attr { Attribute::invalid },
          key_attr  { Attribute::invalid },
          automerge { true },
          process_scope        { new Scope(CALI_SCOPE_PROCESS) },
          default_thread_scope { new Scope(CALI_SCOPE_THREAD)  },
          default_task_scope   { new Scope(CALI_SCOPE_TASK)    }
    {
        automerge = config.get("automerge").to_bool();

        const MetaAttributeIDs* m = tree.meta_attribute_ids();
        
        name_attr = Attribute::make_attribute(tree.node(m->name_attr_id), m);
        type_attr = Attribute::make_attribute(tree.node(m->type_attr_id), m);
        prop_attr = Attribute::make_attribute(tree.node(m->prop_attr_id), m);

        attribute_nodes.insert(make_pair(name_attr.name(), tree.node(name_attr.id())));
        attribute_nodes.insert(make_pair(type_attr.name(), tree.node(type_attr.id())));
        attribute_nodes.insert(make_pair(prop_attr.name(), tree.node(prop_attr.id())));
        
        assert(name_attr != Attribute::invalid);
        assert(type_attr != Attribute::invalid);
        assert(prop_attr != Attribute::invalid);

        pthread_key_create(&thread_scope_key, release_thread);
        pthread_setspecific(thread_scope_key, default_thread_scope);
            
        // now it is safe to use the Caliper interface

        init();
    }

    ~GlobalData() {
        Log(1).stream() << "Finished" << endl;
        
        // prevent re-initialization
        s_init_lock = 2;

	//freeing up context buffers
        delete process_scope;
        delete default_thread_scope; 
        delete default_task_scope;
    }
    
    Scope* acquire_thread_scope(bool create = true) {
        Scope* scope = static_cast<Scope*>(pthread_getspecific(thread_scope_key));

        if (create && !scope) {
            scope = Caliper(this).create_scope(CALI_SCOPE_THREAD);
            pthread_setspecific(thread_scope_key, scope);
        }

        return scope;
    }
    
    void init() {
        Caliper c(this, default_thread_scope, default_task_scope);

        // Create and set key & version attributes

        key_attr =
            c.create_attribute("cali.key.attribute", CALI_TYPE_USR, CALI_ATTR_HIDDEN);
            
        c.set(c.create_attribute("cali.caliper.version", CALI_TYPE_STRING, CALI_ATTR_SCOPE_PROCESS),
              Variant(CALI_TYPE_STRING, CALIPER_VERSION, sizeof(CALIPER_VERSION)));
            
        Services::register_services(&c);

        Log(1).stream() << "Initialized" << endl;

        if (Log::verbosity() >= 2)
            RuntimeConfig::print( Log(2).stream() << "Configuration:\n" );

        c.events().post_init_evt(&c);        
    }
    
    const Attribute&
    get_key(const Attribute& attr) const {
        if (!automerge || attr.store_as_value() || !attr.is_autocombineable())
            return attr;

        return key_attr;
    }

    // temporary - will go away
    void
    write_new_attribute_nodes(WriteRecordFn write_rec) {
        if (new_attributes.exchange(false)) {
            std::lock_guard<std::mutex>
                g(attribute_lock);

            // special handling for bootstrap nodes: write all nodes in-order
            if (!bootstrap_nodes_written.exchange(true))
                for (cali_id_t id = 0; id <= type_attr.id(); ++id) {
                    Node* node = tree.node(id);

                    if (node && !node->check_written())
                        node->push_record(write_rec);
                }
            
            for (auto &p : attribute_nodes)
                p.second->write_path(write_rec);
        }
    }
};

// --- static member initialization

volatile sig_atomic_t  Caliper::GlobalData::s_init_lock = 1;
mutex                  Caliper::GlobalData::s_init_mutex;

Caliper::GlobalData*   Caliper::GlobalData::sG = nullptr;

const ConfigSet::Entry Caliper::GlobalData::s_configdata[] = {
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
// Caliper class definition
//

Caliper::Scope*
Caliper::scope(cali_context_scope_t st) {
    switch (st) {
    case CALI_SCOPE_PROCESS:
        return mG->process_scope;
        
    case CALI_SCOPE_THREAD:
#if 0
        if (!m_thread_scope) {
            m_thread_scope =
                mG->get_thread_scope_cb ? mG->get_thread_scope_cb(this, true) : mG->default_thread_scope;
        }
#endif
        assert(m_thread_scope != 0);
        return m_thread_scope;
        
    case CALI_SCOPE_TASK:
        if (!m_task_scope)
            m_task_scope =
                mG->get_task_scope_cb ? mG->get_task_scope_cb(this, true) : mG->default_task_scope;

        return m_task_scope;
    }

    return mG->process_scope;
}

Caliper::Scope*
Caliper::default_scope(cali_context_scope_t st)
{
    switch (st) {
    case CALI_SCOPE_THREAD:
        return mG->default_thread_scope;
        
    case CALI_SCOPE_TASK:
        return mG->default_task_scope;
        
    default:
        ;
    }

    return mG->process_scope;    
}

void 
Caliper::set_scope_callback(cali_context_scope_t scope, ScopeCallbackFn cb) {
    if (!mG)
        return;
    
    switch (scope) {
    case CALI_SCOPE_THREAD:
        if (mG->get_thread_scope_cb)
            Log(0).stream() 
                << "Caliper::set_context_callback(): error: thread callback already set" 
                << endl;
        mG->get_thread_scope_cb = cb;
        break;
    case CALI_SCOPE_TASK:
        if (mG->get_task_scope_cb)
            Log(0).stream() 
                << "Caliper::set_context_callback(): error: task callback already set" 
                << endl;
        mG->get_task_scope_cb = cb;
        break;
    default:
        Log(0).stream() 
            << "Caliper::set_context_callback(): error: cannot set process callback" 
            << endl;
    }
}

Caliper::Scope*
Caliper::create_scope(cali_context_scope_t st)
{
    assert(mG != 0);

    Scope* s = new Scope(st);
    
    switch (st) {
    case CALI_SCOPE_THREAD:
        m_thread_scope = s;
        break;
    case CALI_SCOPE_TASK:
        m_task_scope   = s;
        break;
    case CALI_SCOPE_PROCESS:
        Log(0).stream()
            << "Caliper::create_scope(): error: attempt to create a process scope"
            << endl;

        delete s;        
        return 0;
    }

    mG->events.create_scope_evt(this, st);

    return s;
}

void
Caliper::release_scope(Caliper::Scope* s)
{
    assert(mG != 0);

    if (Log::verbosity() >= 2) {
        const char* scopestr = "";

        switch (s->scope) {
        case CALI_SCOPE_THREAD:
            scopestr = "thread";
            break;
        case CALI_SCOPE_TASK:
            scopestr = "task";
            break;
        case CALI_SCOPE_PROCESS:
            scopestr = "process";
            break;
        }

        // This will print
        //   "Releasing <process/thread> scope:
        //      <Mempool statistics>
        //      <Blackboard statistics>"
        
        s->blackboard.print_statistics(
            s->mempool.print_statistics(
                Log(2).stream() << "Releasing " << scopestr << " scope:\n      "
                ) << "\n      " ) << std::endl;            
    }
    
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);
    
    mG->events.release_scope_evt(this, s->scope);
    // do NOT delete this because we may still need the node data in the scope's memory pool
    // delete ctx;
}


// --- Attribute interface

Attribute 
Caliper::create_attribute(const std::string& name, cali_attr_type type, int prop,
                          int meta, const Attribute* meta_attr, const Variant* meta_val)
{
    assert(mG != 0);

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);
    
    // Add default SCOPE_THREAD property if no other is set
    if (!(prop & CALI_ATTR_SCOPE_PROCESS) && !(prop & CALI_ATTR_SCOPE_TASK))
        prop |= CALI_ATTR_SCOPE_THREAD;

    Node* node        = nullptr;
    bool  created_now = false;

    // Check if an attribute with this name already exists

    mG->attribute_lock.lock();

    auto it = mG->attribute_nodes.find(name);
    if (it != mG->attribute_nodes.end())
        node = it->second;

    mG->attribute_lock.unlock();

    // Create attribute nodes

    if (!node) {
        mG->events.pre_create_attr_evt(this, name, &type, &prop);

        assert(type >= 0 && type <= CALI_MAXTYPE);
        node = mG->tree.type_node(type);
        assert(node);

        if (meta > 0)
            node = mG->tree.get_path(meta, meta_attr, meta_val, node, &mG->process_scope->mempool);

        Attribute attr[2] { mG->prop_attr, mG->name_attr };
        Variant   data[2] { { prop },      { CALI_TYPE_STRING, name.c_str(), name.size() } };

        if (prop == CALI_ATTR_DEFAULT)
            node = mG->tree.get_path(1, &attr[1], &data[1], node, &mG->process_scope->mempool);
        else
            node = mG->tree.get_path(2, &attr[0], &data[0], node, &mG->process_scope->mempool);

        if (node) {
            // Check again if attribute already exists; might have been created by 
            // another thread in the meantime.
            // We've created some redundant nodes then, but that's fine
            mG->attribute_lock.lock();

            auto it = mG->attribute_nodes.lower_bound(name);

            if (it == mG->attribute_nodes.end() || it->first != name) {
                mG->attribute_nodes.emplace_hint(it, name, node);
                mG->new_attributes.store(true);
                created_now = true;
            } else
                node = it->second;

            mG->attribute_lock.unlock();
        }
    }

    // Create attribute object

    Attribute attr = Attribute::make_attribute(node, mG->tree.meta_attribute_ids());

    if (created_now)
        mG->events.create_attr_evt(this, attr);

    return attr;
}

Attribute
Caliper::get_attribute(const string& name) const
{
    assert(mG != 0);

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);
    
    Node* node = nullptr;

    mG->attribute_lock.lock();

    auto it = mG->attribute_nodes.find(name);

    if (it != mG->attribute_nodes.end())
        node = it->second;

    mG->attribute_lock.unlock();

    return Attribute::make_attribute(node, mG->tree.meta_attribute_ids());
}

Attribute 
Caliper::get_attribute(cali_id_t id) const
{
    assert(mG != 0);
    // no signal lock necessary
    
    return Attribute::make_attribute(mG->tree.node(id), mG->tree.meta_attribute_ids());
}


// --- Snapshot interface

void
Caliper::pull_snapshot(int scopes, const EntryList* trigger_info, EntryList* sbuf)
{
    assert(mG != 0);

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    // Save trigger info in snapshot buf

    if (trigger_info)
        sbuf->append(*trigger_info);

    // Invoke callbacks and get contextbuffer data

    mG->events.snapshot(this, scopes, trigger_info, sbuf);

    for (cali_context_scope_t s : { CALI_SCOPE_TASK, CALI_SCOPE_THREAD, CALI_SCOPE_PROCESS })
        if (scopes & s)
            scope(s)->blackboard.snapshot(sbuf);
}

void 
Caliper::push_snapshot(int scopes, const EntryList* trigger_info)
{
    assert(mG != 0);
    
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    EntryList::FixedEntryList<64> snapshot_data;
    EntryList sbuf(snapshot_data);

    pull_snapshot(scopes, trigger_info, &sbuf);

    if (!m_is_signal)
        mG->write_new_attribute_nodes(mG->events.write_record);

    mG->events.process_snapshot(this, trigger_info, &sbuf);
}

void
Caliper::flush(const EntryList* entry)
{
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    mG->events.flush(this, entry);
}

// --- Annotation interface

cali_err 
Caliper::begin(const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_EINV;

    if (!mG || attr == Attribute::invalid)
        return CALI_EINV;

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    // invoke callbacks
    if (!attr.skip_events())
        mG->events.pre_begin_evt(this, attr, data);

    Scope* s = scope(attr2caliscope(attr));
    ContextBuffer* sb = &s->blackboard;
    
    if (attr.store_as_value())
        ret = sb->set(attr, data);
    else
        ret = sb->set_node(mG->get_key(attr),
                           mG->tree.get_path(1, &attr, &data,
                                             sb->get_node(mG->get_key(attr)),
                                             &s->mempool));

    // invoke callbacks
    if (!attr.skip_events())
        mG->events.post_begin_evt(this, attr, data);

    return ret;
}

cali_err 
Caliper::end(const Attribute& attr)
{
    if (!mG || attr == Attribute::invalid)
        return CALI_EINV;

    cali_err ret = CALI_EINV;

    Scope* s = scope(attr2caliscope(attr));
    ContextBuffer* sb = &s->blackboard;

    Variant val;

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    // invoke callbacks
    if (!attr.skip_events()) {
        val = get(attr).value();
        mG->events.pre_end_evt(this, attr, val);
    }
    
    if (attr.store_as_value())
        ret = sb->unset(attr);
    else {
        Node* node = sb->get_node(mG->get_key(attr));

        if (node) {
            node = mG->tree.remove_first_in_path(node, attr, &s->mempool);
                
            if (node == mG->tree.root())
                ret = sb->unset(mG->get_key(attr));
            else if (node)
                ret = sb->set_node(mG->get_key(attr), node);
        }

        if (!node)
            Log(0).stream() << "error: trying to end inactive attribute " << attr.name() << endl;
    }

    // invoke callbacks
    if (!attr.skip_events())
        mG->events.post_end_evt(this, attr, val);

    return ret;
}

cali_err 
Caliper::set(const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_EINV;

    if (!mG || attr == Attribute::invalid)
        return CALI_EINV;

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    Scope* s = scope(attr2caliscope(attr));
    ContextBuffer* sb = &s->blackboard;

    // invoke callbacks
    if (!attr.skip_events())
        mG->events.pre_set_evt(this, attr, data);

    if (attr.store_as_value())
        ret = sb->set(attr, data);
    else {
        Attribute key = mG->get_key(attr);
        
        ret = sb->set_node(key, mG->tree.replace_first_in_path(sb->get_node(key), attr, data, &s->mempool));
    }
    
    // invoke callbacks
    if (!attr.skip_events())
        mG->events.post_set_evt(this, attr, data);

    return ret;
}

cali_err 
Caliper::set_path(const Attribute& attr, size_t n, const Variant* data) {
    cali_err ret = CALI_EINV;

    if (n < 1)
        return CALI_SUCCESS;    
    if (!mG || attr == Attribute::invalid)
        return CALI_EINV;

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    Scope* s = scope(attr2caliscope(attr));
    ContextBuffer* sb = &s->blackboard;

    // invoke callbacks
    if (!attr.skip_events())
        mG->events.pre_set_evt(this, attr, data[n-1]);

    if (attr.store_as_value()) {
        Log(0).stream() << "error: set_path() invoked with immediate-value attribute " << attr.name() << endl;
        ret = CALI_EINV;
    } else {
        Attribute key = mG->get_key(attr);
        
        ret = sb->set_node(key,
                           mG->tree.replace_all_in_path(sb->get_node(key), attr, n, data, &s->mempool));
    }
    
    // invoke callbacks
    if (!attr.skip_events())
        mG->events.post_set_evt(this, attr, data[n-1]);

    return ret;
}

// --- Query

Entry
Caliper::get(const Attribute& attr) 
{
    Entry e {  Entry::empty };

    if (!mG || attr == Attribute::invalid)
        return Entry::empty;

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    ContextBuffer* sb = &(scope(attr2caliscope(attr))->blackboard);

    if (attr.store_as_value())
        return Entry(attr, sb->get(attr));
    else
        return Entry(mG->tree.find_node_with_attribute(attr, sb->get_node(mG->get_key(attr))));

    return e;
}

// --- Generic entry API

void
Caliper::make_entrylist(size_t n, const Attribute* attr, const Variant* value, EntryList& list) 
{
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    Node* node = 0;

    for (size_t i = 0; i < n; ++i)
        if (attr[i].store_as_value())
            list.append(attr[i].id(), value[i]);
        else
            node = mG->tree.get_path(1, &attr[i], &value[i], node, &(scope(attr2caliscope(attr[i]))->mempool));

    if (node)
        list.append(node);
}

Entry 
Caliper::make_entry(const Attribute& attr, const Variant& value) 
{
    if (attr == Attribute::invalid)
        return Entry::empty;
    
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    Entry entry { Entry::empty };

    if (attr.store_as_value())
        return Entry(attr, value);
    else
        return Entry(mG->tree.get_path(1, &attr, &value, nullptr, &(scope(attr2caliscope(attr))->mempool)));

    return entry;
}

Node*
Caliper::node(cali_id_t id)
{
    // no siglock necessary
    return mG->tree.node(id);
}

// --- Events interface

Caliper::Events&
Caliper::events()
{
    return mG->events;
}

Variant
Caliper::exchange(const Attribute& attr, const Variant& data)
{
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    return scope(attr2caliscope(attr))->blackboard.exchange(attr, data);
}


//
// --- Caliper constructor & singleton API
//

Caliper::Caliper()
    : mG(0), m_thread_scope(0), m_task_scope(0)
{
    *this = Caliper::instance();
}

Caliper
Caliper::instance()
{
    if (GlobalData::s_init_lock != 0) {
        if (GlobalData::s_init_lock == 2)
            // Caliper had been initialized previously; we're past the static destructor
            return Caliper(0);

        lock_guard<mutex> lock(GlobalData::s_init_mutex);

        if (!GlobalData::sG) {
            if (atexit(::exit_handler) != 0)
                Log(0).stream() << "Unable to register exit handler";

            GlobalData::sG = new Caliper::GlobalData;
            
            GlobalData::s_init_lock = 0;
        }
    }

    return Caliper(GlobalData::sG, GlobalData::sG->acquire_thread_scope());
}

Caliper
Caliper::sigsafe_instance()
{
    if (GlobalData::s_init_lock != 0)
        return Caliper(0);

    Scope* task_scope   = 0; // FIXME: figure out task scope 
    Scope* thread_scope = GlobalData::sG->acquire_thread_scope(false);

    if (!thread_scope || thread_scope->lock.is_locked())
        return Caliper(0);

    return Caliper(GlobalData::sG, thread_scope, task_scope, true /* is signal */);
}

void
Caliper::release()
{
    delete GlobalData::sG;
    GlobalData::sG = 0;
}

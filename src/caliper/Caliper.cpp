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

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "ContextBuffer.h"
#include "MetadataTree.h"

#include "caliper/common/ContextRecord.h"
#include "caliper/common/Node.h"
#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/util/split.hpp"

#include "../services/Services.h"

#include <signal.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <utility>

using namespace cali;
using namespace std;

namespace cali
{
    extern void init_attribute_classes(Caliper* c);
    extern void init_api_attributes(Caliper* c);
}

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
            c.flush_and_write(nullptr);
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
    MetadataTree         tree;
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

    mutable std::mutex     attribute_lock;
    map<string, Node*>     attribute_nodes;
    map<string, int>       attribute_prop_presets;

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
        
        name_attr = Attribute::make_attribute(default_thread_scope->tree.node( 8));
        type_attr = Attribute::make_attribute(default_thread_scope->tree.node( 9));
        prop_attr = Attribute::make_attribute(default_thread_scope->tree.node(10));

        attribute_nodes.insert(make_pair(name_attr.name(), default_thread_scope->tree.node(name_attr.id())));
        attribute_nodes.insert(make_pair(type_attr.name(), default_thread_scope->tree.node(type_attr.id())));
        attribute_nodes.insert(make_pair(prop_attr.name(), default_thread_scope->tree.node(prop_attr.id())));
        
        assert(name_attr != Attribute::invalid);
        assert(type_attr != Attribute::invalid);
        assert(prop_attr != Attribute::invalid);

        pthread_key_create(&thread_scope_key, release_thread);
        pthread_setspecific(thread_scope_key, default_thread_scope);

        parse_attribute_property_presets();
        
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

    void parse_attribute_property_presets() {
        vector<string> attr_props;

        util::split(config.get("attribute_properties").to_string(), ',', back_inserter(attr_props));

        for (const string& s : attr_props) {
            auto p = s.find_first_of('=');

            if (p == string::npos)
                continue;

            int prop = cali_string2prop(s.substr(p+1).c_str());

            attribute_prop_presets.insert(make_pair(s.substr(0, p), prop));
        }
    }
    
    void init() {
        Caliper c(this, default_thread_scope, default_task_scope);
        
        // Create and set key & version attributes

        key_attr =
            c.create_attribute("cali.key.attribute", CALI_TYPE_USR, CALI_ATTR_HIDDEN);
            
        c.set(c.create_attribute("cali.caliper.version", CALI_TYPE_STRING, CALI_ATTR_SCOPE_PROCESS),
              Variant(CALI_TYPE_STRING, CALIPER_VERSION, sizeof(CALIPER_VERSION)));

        init_attribute_classes(&c);

        Services::register_services(&c);

        init_api_attributes(&c);
        
        Log(1).stream() << "Initialized" << endl;

        if (Log::verbosity() >= 3)
            RuntimeConfig::print( Log(3).stream() << "Configuration:\n" );

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
                    Node* node = default_thread_scope->tree.node(id);

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
    { "attribute_properties", CALI_TYPE_STRING, "",
      "List of attribute property presets",
      "List of attribute property presets, in the form\n"
      "  attr=prop1:prop2,attr2=prop1:prop2:prop3,attr3=prop1,...\n"
      "Attribute property flags are:\n"
      "  asvalue:       Store values directly in snapshot, not in context tree\n"
      "  nomerge:       Create dedicated context tree branch, don't merge with other attributes\n"
      "  process_scope: Process-scope attribute\n"
      "  thread_scope:  Thread-scope attribute\n"
      "  task_scope:    Task-scope attribute (currently not supported)\n"
      "  skip_events:   Do not invoke callback functions for updates\n"
      "  hidden:        Do not include this attribute in snapshots\n" 
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
        //      <Blackboard statistics>"
        //      <Metadata tree statistics>

        s->tree.print_statistics(        
            s->blackboard.print_statistics(
                Log(2).stream() << "Releasing " << scopestr << " scope:\n      " ) 
            << "\n      ") << std::endl;
    }
    
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);
    
    mG->events.release_scope_evt(this, s->scope);
    // do NOT delete this because we may still need the node data in the scope's memory pool
    // delete ctx;
}


// --- Attribute interface

/// \brief Create an attribute
/// 
/// This function creates and returns an attribute key with the given name, type, and properties.
/// Optionally, metadata can be added via attribute:value pairs.
/// Attribute names must be unique. If an attribute with the given name already exists, the 
/// existing attribute is returned.
///
/// Before a new attribute is created, the pre_create_attr_evt callback will be invoked, 
/// which allows modifications of the user-provided parameters (such as the property flags).
/// After a new attribute has been created, this function will invoke the create_attr_evt callback.
/// If an attribute with the given name already exists, the callbacks will not be invoked.
/// If two threads create an attribute with the same name simultaneously, the pre_create_attr_evt
/// callback may be invoked on both threads, but create_attr_evt will only be invoked once.
/// However, both threads will successfully return the new attribute.
///
/// This function is not signal safe.
/// 
/// \param name Name of the attribute
/// \param type Type of the attribute
/// \param prop Attribute property bitmap. Values of type cali_attr_properties combined with bitwise or.
/// \param n_meta Number of metadata entries
/// \param meta_attr Metadata attribute list. An array of n_meta attribute entries.
/// \param meta_val Metadata values. An array of n_meta values.
/// \return The created attribute.

Attribute 
Caliper::create_attribute(const std::string& name, cali_attr_type type, int prop,
                          int n_meta, const Attribute* meta_attr, const Variant* meta_val)
{
    assert(mG != 0);

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);    

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
        // Get type node
        assert(type >= 0 && type <= CALI_MAXTYPE);
        node = m_thread_scope->tree.type_node(type);
        assert(node);

        // Add metadata nodes.
        if (n_meta > 0)
            node = m_thread_scope->tree.get_path(n_meta, meta_attr, meta_val, node);

        // Look for attribute properties in presets 
        auto propit = mG->attribute_prop_presets.find(name);
        if (propit != mG->attribute_prop_presets.end())
            prop = propit->second;

        // Run pre-attribute creation callbacks. This may add additional to our parent node.
        mG->events.pre_create_attr_evt(this, name, type, &prop, &node);
        
        // Add default SCOPE_THREAD property if no other is set
        if (((prop & CALI_ATTR_SCOPE_MASK) != CALI_ATTR_SCOPE_PROCESS) &&
            ((prop & CALI_ATTR_SCOPE_MASK) != CALI_ATTR_SCOPE_TASK))
            prop |= CALI_ATTR_SCOPE_THREAD;

        Attribute attr[2] { mG->prop_attr, mG->name_attr };
        Variant   data[2] { { prop },      { CALI_TYPE_STRING, name.c_str(), name.size() } };

        if (prop == CALI_ATTR_DEFAULT)
            node = m_thread_scope->tree.get_path(1, &attr[1], &data[1], node);
        else
            node = m_thread_scope->tree.get_path(2, &attr[0], &data[0], node);

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

    Attribute attr = Attribute::make_attribute(node);

    if (created_now)
        mG->events.create_attr_evt(this, attr);

    return attr;
}

/// \brief Find an attribute by name
///
/// While it should be signal safe, we do not recommend using this function in a signal handler.
///
/// \param name The attribute name
/// \return Attribute object, or Attribute::invalid if not found.

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

    return Attribute::make_attribute(node);
}

/// \brief Find attribute by id
/// \note This function is signal safe.
/// \param id The attribute id
/// \return Attribute object, or Attribute::invalid if not found.

Attribute 
Caliper::get_attribute(cali_id_t id) const
{
    // no signal lock necessary
    
    return Attribute::make_attribute(m_thread_scope->tree.node(id));
}

/// \brief Get all attributes.
/// \note This function is _not_ signal safe.
/// \param id The attribute id
/// \return   A vector that containing all attribute objects

std::vector<Attribute>
Caliper::get_attributes() const
{
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);
    std::lock_guard<std::mutex>
        g_a(mG->attribute_lock);

    std::vector<Attribute> ret;
    ret.reserve(mG->attribute_nodes.size());

    for (auto it : mG->attribute_nodes)
        ret.push_back(Attribute::make_attribute(it.second));

    return ret;
}

// --- Snapshot interface

/// \brief Trigger and return a snapshot. 
///
/// This function triggers a snapshot and returns a snapshot record to the caller.
/// The returned snapshot record contains the current blackboard contents, measurement 
/// values provided by service modules, and the contents of the trigger_info list 
/// provided by the caller.
///
/// The function invokes the snapshot callback, which instructs attached services to 
/// take measurements (e.g., a timestamp) and add them to the returned record. The 
/// caller-provided trigger_info list is passed to the snapshot callback.
/// The returned snapshot record also contains contents of the current thread's and the  
/// process-wide blackboard, as specified in the scopes flag.
///
/// The caller must provide a snapshot buffer with sufficient free space.
///
/// \note This function is signal safe.
///
/// \param scopes       Specifies which blackboard(s) contents to put into the snapshot buffer. 
///                     Bitfield of cali_scope_t values combined with bitwise OR.
/// \param trigger_info A caller-provided list of attributes that is passed to the snapshot
///                     callback, and added to the returned snapshot record.
/// \param sbuf         Caller-provided snapshot record buffer in which the snapshot record is
///                     returned. Must have sufficient space for the snapshot contents.
void
Caliper::pull_snapshot(int scopes, const SnapshotRecord* trigger_info, SnapshotRecord* sbuf)
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

/// \brief Trigger and process a snapshot. 
///
/// This function triggers a snapshot and processes it. The snapshot contains the 
/// current blackboard contents, measurement values provided by service modules, 
/// and the contents of the trigger_info list provided by the caller.
/// The complete snapshot is then passed to snapshot processing services registered
/// with Caliper.
///
/// The function creates a snapshot record with measurements provided by the snapshot callbac, 
/// the current thread's and/or processes' blackboard contents, as well as the contents of 
/// the trigger_info list provided by the caller.
///
/// The function invokes the snapshot callback to obtain measurements.
/// The fully assembled snapshot record is then passed to the process_snapshot callback.  
///
/// \note This function is signal safe.
///
/// \param scopes       Specifies which blackboard(s) contents to put into the snapshot buffer. 
///                     Bitfield of cali_scope_t values combined with bitwise OR.
/// \param trigger_info A caller-provided list of attributes that is passed to the snapshot
///                     and process_snapshot callbacks, and added to the returned snapshot record.

void 
Caliper::push_snapshot(int scopes, const SnapshotRecord* trigger_info)
{
    assert(mG != 0);
    
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    SnapshotRecord::FixedSnapshotRecord<80> snapshot_data;
    SnapshotRecord sbuf(snapshot_data);

    pull_snapshot(scopes, trigger_info, &sbuf);

    mG->events.process_snapshot(this, trigger_info, &sbuf);
}


/// \brief Flush aggregation/trace buffer contents.
void
Caliper::flush(const SnapshotRecord* flush_info, SnapshotFlushFn proc_fn)
{
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    mG->events.pre_flush_evt(this, flush_info);

    if (mG->events.postprocess_snapshot.empty()) { 
        mG->events.flush_evt(this, flush_info, proc_fn);
    } else {
        mG->events.flush_evt(this, flush_info, 
                             [this,flush_info,proc_fn](const SnapshotRecord* input_snapshot) {
                                 SnapshotRecord::FixedSnapshotRecord<80> data;
                                 SnapshotRecord snapshot(data);

                                 snapshot.append(*input_snapshot);

                                 mG->events.postprocess_snapshot(this, &snapshot);
                                 return proc_fn(&snapshot);
                             });
    }
}


/// Forward aggregation/trace buffer contents to output services.
///
/// Flushes trace buffers and / or the aggregation database in the trace and aggregation
/// services, respectively. This will empty the trace/aggregation buffers and
/// forward all buffered snapshot records to output services, e.g., report and recorder.
///
/// This function will invoke the pre_flush, flush, and flush_finish callbacks.
/// \note This function is not signal safe.
///
/// \param input_flush_info User-provided flush context information. Currently unused.

void
Caliper::flush_and_write(const SnapshotRecord* input_flush_info)
{
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    SnapshotRecord::FixedSnapshotRecord<80> snapshot_data;
    SnapshotRecord flush_info(snapshot_data);

    if (input_flush_info)
        flush_info.append(*input_flush_info);

    m_thread_scope->blackboard.snapshot(&flush_info);
    mG->process_scope->blackboard.snapshot(&flush_info);

    mG->events.pre_write_evt(this, &flush_info);

    flush(&flush_info, 
          [this,&flush_info](const SnapshotRecord* snapshot){
              mG->events.write_snapshot(this, &flush_info, snapshot);
              return true;
          });

    mG->events.post_write_evt(this, &flush_info);
}


// --- Annotation interface

/// \brief Push attribute:value pair on blackboard.
///
/// Adds the given attribute/value pair on the blackboard. Appends
/// the value to any previous values of the same attribute,
/// creating a hierarchy.
///
/// This function invokes pre_begin/post_begin callbacks, unless the
/// CALI_ATTR_SKIP_EVENTS attribute property is set in `attr`.
///
/// This function is signal safe.
///
/// \param attr Attribute key
/// \param data Value to set

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
                           m_thread_scope->tree.get_path(1, &attr, &data,
                                                         sb->get_node(mG->get_key(attr))));

    // invoke callbacks
    if (!attr.skip_events())
        mG->events.post_begin_evt(this, attr, data);

    return ret;
}

/// \brief Pop/remove top-most entry with given attribute from blackboard.
///
/// This function invokes the pre_end/post_end callbacks, unless the
/// CALI_ATTR_SKIP_EVENTS attribute property is set in \a attr.
///
/// This function is signal safe.
///
/// \param attr Attribute key.

cali_err 
Caliper::end(const Attribute& attr)
{
    if (!mG || attr == Attribute::invalid)
        return CALI_EINV;

    cali_err ret = CALI_EINV;

    Scope* s = scope(attr2caliscope(attr));
    ContextBuffer* sb = &s->blackboard;

    Entry  e = get(attr);

    if (e.is_empty())
        return CALI_ESTACK;

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    // invoke callbacks
    if (!attr.skip_events())
        if (!e.is_empty()) // prevent executing events for 
            mG->events.pre_end_evt(this, attr, e.value());

    if (attr.store_as_value())
        ret = sb->unset(attr);
    else {
        Node* node = sb->get_node(mG->get_key(attr));

        if (node) {
            node = m_thread_scope->tree.remove_first_in_path(node, attr);
                
            if (node == m_thread_scope->tree.root())
                ret = sb->unset(mG->get_key(attr));
            else if (node)
                ret = sb->set_node(mG->get_key(attr), node);
        }

        if (!node)
            Log(0).stream() << "error: trying to end inactive attribute " << attr.name() << endl;
    }

    // invoke callbacks
    if (!attr.skip_events())
        mG->events.post_end_evt(this, attr, e.value());

    return ret;
}

/// \brief Set attribute:value pair on blackboard.
///
/// Set the given attribute/value pair on the blackboard. Overwrites
/// the previous values of the same attribute.
///
/// This function invokes pre_set/post_set callbacks, unless the
/// CALI_ATTR_SKIP_EVENTS attribute property is set in \a attr.
///
/// This function is signal safe.
///
/// \param attr Attribute key
/// \param data Value to set

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
        
        ret = sb->set_node(key, m_thread_scope->tree.replace_first_in_path(sb->get_node(key), attr, data));
    }
    
    // invoke callbacks
    if (!attr.skip_events())
        mG->events.post_set_evt(this, attr, data);

    return ret;
}

/// \brief Set a list of values for attribute \a attr blackboard.
///
/// Sets the given values on the blackboard. Overwrites
/// the previous values of the same attribute.
///
/// This function invokes pre_set/post_set callbacks, unless the
/// CALI_ATTR_SKIP_EVENTS attribute property is set in \a attr.
///
/// This function is signal safe.
///
/// \param attr Attribute key
/// \param n    Number of values in list
/// \param data List (array) of values

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
                           m_thread_scope->tree.replace_all_in_path(sb->get_node(key), attr, n, data));
    }
    
    // invoke callbacks
    if (!attr.skip_events())
        mG->events.post_set_evt(this, attr, data[n-1]);

    return ret;
}

// --- Query

/// \brief Retrieve top-most entry for the given attribute key from the blackboard.
///
/// This function is signal safe.
///
/// \param attr Attribute key.
///
/// \return The top-most entry on the blackboard for the given attribute key.
///         An empty Entry object if this attribute is not set.

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
        return Entry(m_thread_scope->tree.find_node_with_attribute(attr, sb->get_node(mG->get_key(attr))));

    return e;
}

// --- Generic entry API

/// \brief Create a snapshot record (entry list) from the given attribute:value pairs
///
/// This function is signal-safe.
///
/// \param n      Number of elements in attribute/value lists
/// \param attr   Attribute list
/// \param value  Value list
/// \param list   Output record. Must be large enough to hold all entries.

void
Caliper::make_entrylist(size_t n, const Attribute* attr, const Variant* value, SnapshotRecord& list) 
{
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    Node* node = 0;

    for (size_t i = 0; i < n; ++i)
        if (attr[i].store_as_value())
            list.append(attr[i].id(), value[i]);
        else
            node = m_thread_scope->tree.get_path(1, &attr[i], &value[i], node);

    if (node)
        list.append(node);
}

/// \brief Create an Entry structure from the given attribute:value pair.
///
/// This function is signal safe.
///
/// \param attr  Attribute key
/// \param value Value
///
/// \retrun Entry object. 

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
        return Entry(m_thread_scope->tree.get_path(1, &attr, &value, nullptr));

    return entry;
}

/// \brief Return a context tree path for the key:value pairs from a given list of
/// nodes.
///
/// This function is signal safe.
///
/// \param n Number of nodes in node list
/// \param nodelist List of nodes to take key:value pairs from
/// \param parent   Construct path off this parent node
///
/// \return Node pointing to the end of the new path

Node*
Caliper::make_tree_entry(size_t n, const Node* nodelist[], Node* parent)
{
    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    return m_thread_scope->tree.get_path(n, nodelist, parent);
}

/// \brief Return a context tree path for the given key:value pairs.
///
/// \note This function is signal safe.
///
/// \param n Number of nodes in node list
/// \param attr   Attribute. Cannot have the AS VALUE property.
/// \param data   Value 
/// \param parent Construct path off this parent node
///
/// \return Node pointing to the end of the new path

Node*
Caliper::make_tree_entry(const Attribute& attr, const Variant& data, Node*  parent)
{
    if (attr.store_as_value())
        return nullptr;

    std::lock_guard<::siglock>
        g(m_thread_scope->lock);

    return m_thread_scope->tree.get_path(1, &attr, &data, parent);
}

/// \brief Return the node with the given id.
///
/// This function is signal safe.
///
/// \return The node. Null if the node was not found.

Node*
Caliper::node(cali_id_t id) const
{
    // no siglock necessary
    return m_thread_scope->tree.node(id);
}

// --- Events interface

Caliper::Events&
Caliper::events()
{
    return mG->events;
}

/// Exchange value on the blackboard. Atomically updates value for given
/// attribute key and returns the previous value.
///
/// This function is signal safe.
///
/// \param attr Attribute key. Must have AS_VALUE attribute property.
/// \param data The new value.
///
/// \return The previous value for the given key.

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

/// \brief Construct a Caliper instance object.
/// \see instance()

Caliper::Caliper()
    : mG(0), m_thread_scope(0), m_task_scope(0)
{
    *this = Caliper::instance();
}

/// \brief Construct a Caliper instance object.
///
/// The Caliper instance object provides access to the Caliper API.
/// Internally, Caliper maintains a variety of thread-local data structures.
/// The instance object caches access to these structures. As a result,
/// one cannot share Caliper instance objects between threads.
/// We recommend to use Caliper instance objects only within a function context
/// (i.e., on the stack).
///
/// For use within signal handlers, use `sigsafe_instance()`.
/// \see sigsafe_instance()
///
/// Caliper will initialize itself in the first instance object request on a
/// process.
/// 
/// \return Caliper instance object

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

/// \brief Construct a signal-safe Caliper instance object.
///
/// A signal-safe Caliper instance object will have a flag set to instruct
/// the API and services that only signal-safe operations can be used.
///
/// \see instance()
///
/// \return Caliper instance object

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

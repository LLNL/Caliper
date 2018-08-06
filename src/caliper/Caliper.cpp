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

#include "../services/Services.h"

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

namespace cali
{

extern void init_attribute_classes(Caliper* c);
extern void init_api_attributes(Caliper* c);

extern void config_sanity_check(RuntimeConfig);

}

namespace
{

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
// Caliper experiment data
//

struct Experiment::ExperimentImpl
{
    static const ConfigSet::Entry   s_configdata[];

    struct ThreadData {
        std::vector<ContextBuffer>  exp_blackboards;

        ThreadData(size_t min_num_entries)
            {
                exp_blackboards.resize(std::max(static_cast<size_t>(16), min_num_entries));
            }
    };

    static thread_local std::unique_ptr<ThreadData> sT;

    std::string                     name;
    bool                            active;
    
    // TODO: call events.create_scope_evt when creating new thread

    RuntimeConfig                   config;

    Events                          events;          ///< callbacks
    ContextBuffer                   blackboard;      ///< process-wide blackboard

    bool                            automerge;
    bool                            flush_on_exit;

    map<string, int>                attribute_prop_presets;

    ExperimentImpl(const char* _name, const RuntimeConfig& cfg)
        : name(_name), active(true), config(cfg)
        {
            ConfigSet cali_cfg =
                config.init("caliper", s_configdata);

            automerge     = cali_cfg.get("automerge").to_bool();
            flush_on_exit = cali_cfg.get("flush_on_exit").to_bool();
        }

    ~ExperimentImpl()
        {
            if (Log::verbosity() >= 2) {
                blackboard.print_statistics(
                    Log(2).stream() << "Releasing experiment " << name << ":\n  ")
                    << std::endl;
            }
        }

    void parse_attribute_property_presets() {
        vector<string> attr_props =
            config.get("caliper", "attribute_properties").to_stringlist();

        for (const string& s : attr_props) {
            auto p = s.find_first_of('=');

            if (p == string::npos)
                continue;

            int prop = cali_string2prop(s.substr(p+1).c_str());

            attribute_prop_presets.insert(make_pair(s.substr(0, p), prop));
        }
    }

    const Attribute&
    get_key(const Attribute& attr, const Attribute& key_attr) const {
        if (!automerge || attr.store_as_value() || !attr.is_autocombineable())
            return attr;

        return key_attr;
    }

    ContextBuffer*
    get_blackboard(const Experiment* exp, const Attribute& attr) {        
        switch (attr.properties() & CALI_ATTR_SCOPE_MASK)
        {
        case CALI_ATTR_SCOPE_THREAD:
        case CALI_ATTR_SCOPE_TASK:
        {
            cali_id_t exp_id = exp->id();
            
            if (sT->exp_blackboards.size() > exp_id)
                return &sT->exp_blackboards[exp_id];
            else
                return nullptr;
        }
        break;
        case CALI_ATTR_SCOPE_PROCESS:
            return &blackboard;
        }
        
        return nullptr;
    }
    
    ContextBuffer*
    get_blackboard(const Experiment* exp, cali_context_scope_t scope) {
        cali_id_t exp_id = exp->id();
        
        switch (scope)
        {
        case CALI_SCOPE_THREAD:
        case CALI_SCOPE_TASK:
            if (sT->exp_blackboards.size() > exp_id)
                return &sT->exp_blackboards[exp_id];
            else
                return nullptr;
        break;
        case CALI_SCOPE_PROCESS:
            return &blackboard;
        }
        
        return nullptr;
    }
};

const ConfigSet::Entry Experiment::ExperimentImpl::s_configdata[] = {
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
      "  nested:        Values are properly nested with the call stack and other nested attributes\n"
    },
    { "config_check", CALI_TYPE_BOOL, "true",
      "Perform configuration sanity check at initialization",
      "Perform configuration sanity check at initialization"
    },
    { "flush_on_exit", CALI_TYPE_BOOL, "true",
      "Flush Caliper buffers at program exit",
      "Flush Caliper buffers at program exit"
    },
    ConfigSet::Terminator
};

thread_local std::unique_ptr<Experiment::ExperimentImpl::ThreadData> Experiment::ExperimentImpl::sT;

Experiment::Experiment(cali_id_t id, const char* name, const RuntimeConfig& cfg)
    : IdType(id),
      mP(new ExperimentImpl(name, cfg))
{
}

Experiment::~Experiment()
{
}

Experiment::Events&
Experiment::events()
{
    return mP->events;
}

RuntimeConfig
Experiment::config()
{
    return mP->config;
}

std::string
Experiment::name() const
{
    return mP->name;
}

bool
Experiment::is_active() const
{
    return mP->active;
}


//
// --- Caliper thread data
//

/// \brief Per-thread data for the Caliper object
struct Caliper::ThreadData
{
    MetadataTree                  tree;
    ::siglock                     lock;
};


//
// --- Caliper Global Data
//

struct Caliper::GlobalData
{
    // --- static data

    static volatile sig_atomic_t          s_init_lock;
    static std::mutex                     s_init_mutex;

    struct InitHookList {
        void          (*hook)();
        InitHookList* next;
    };

    static InitHookList*                  s_init_hooks;
    
    // --- static functions

    static void add_init_hook(void (*hook)()) {
        InitHookList* elem = new InitHookList { hook, s_init_hooks };
        s_init_hooks = elem;
    }

    static void run_init_hooks() {
        for (InitHookList* lp = s_init_hooks; lp; lp = lp->next)
            (*(lp->hook))();
    }

    // --- data

    mutable std::mutex                    attribute_lock;
    map<string, Node*>                    attribute_nodes;

    Attribute                             key_attr;

    vector< std::unique_ptr<Experiment> > experiments;

    // --- constructor

    GlobalData()
        : key_attr { Attribute::invalid }
    {        
        experiments.reserve(16);

        // put the attribute [name,type,prop] attributes in the map
        
        Attribute name_attr =
            Attribute::make_attribute(sT->tree.node( 8));
        Attribute type_attr =
            Attribute::make_attribute(sT->tree.node( 9));
        Attribute prop_attr =
            Attribute::make_attribute(sT->tree.node(10));

        attribute_nodes.insert(make_pair(name_attr.name(),
                                         sT->tree.node(name_attr.id())));
        attribute_nodes.insert(make_pair(type_attr.name(),
                                         sT->tree.node(type_attr.id())));
        attribute_nodes.insert(make_pair(prop_attr.name(),
                                         sT->tree.node(prop_attr.id())));
    }

    ~GlobalData() {
        Log(1).stream() << "Finished" << endl;

        // prevent re-initialization
        s_init_lock = 2;
    }

    void init() {
        run_init_hooks();
        
        Services::add_default_services();
        
        Caliper c(false);
        
        key_attr =
            c.create_attribute("cali.key.attribute", CALI_TYPE_USR, CALI_ATTR_HIDDEN);
        
        init_attribute_classes(&c);
        init_api_attributes(&c);
        
        c.create_experiment("default", RuntimeConfig::get_default_config());

        Log(1).stream() << "Initialized" << std::endl;
    }
};

// --- static member initialization

volatile sig_atomic_t  Caliper::GlobalData::s_init_lock = 1;
mutex                  Caliper::GlobalData::s_init_mutex;

std::unique_ptr<Caliper::GlobalData> Caliper::sG;
thread_local std::unique_ptr<Caliper::ThreadData> Caliper::sT;

Caliper::GlobalData::InitHookList* Caliper::GlobalData::s_init_hooks = nullptr;

//
// Caliper class definition
//

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
    assert(sG != 0);

    std::lock_guard<::siglock>
        g(sT->lock);
    
    Attribute name_attr =
        Attribute::make_attribute(sT->tree.node( 8));
    Attribute type_attr =
        Attribute::make_attribute(sT->tree.node( 9));
    Attribute prop_attr =
        Attribute::make_attribute(sT->tree.node(10));

    assert(name_attr != Attribute::invalid);
    assert(type_attr != Attribute::invalid);
    assert(prop_attr != Attribute::invalid);

    Node* node        = nullptr;
    bool  created_now = false;

    // Check if an attribute with this name already exists

    sG->attribute_lock.lock();

    auto it = sG->attribute_nodes.find(name);
    if (it != sG->attribute_nodes.end())
        node = it->second;

    sG->attribute_lock.unlock();

    // Create attribute nodes

    if (!node) {
        // Get type node
        assert(type >= 0 && type <= CALI_MAXTYPE);
        node = sT->tree.type_node(type);
        assert(node);

        // Add metadata nodes.
        if (n_meta > 0)
            node = sT->tree.get_path(n_meta, meta_attr, meta_val, node);

        // // Look for attribute properties in presets
        // auto propit = sG->attribute_prop_presets.find(name);
        // if (propit != sG->attribute_prop_presets.end())
        //     prop = propit->second;

        // Run pre-attribute creation callbacks.
        //   This may add additional nodes to our parent branch.
        for (auto& exp : sG->experiments)
            if (exp)
                exp->mP->events.pre_create_attr_evt(this, exp.get(), name, type, &prop, &node);

        // Set scope to PROCESS for all global attributes
        if (prop & CALI_ATTR_GLOBAL) {
            prop &= ~CALI_ATTR_SCOPE_MASK;
            prop |= CALI_ATTR_SCOPE_PROCESS;
        }
        // Set THREAD scope if none is set
        if ((prop & CALI_ATTR_SCOPE_MASK) == 0)
            prop |= CALI_ATTR_SCOPE_THREAD;

        Attribute attr[2] { prop_attr, name_attr };
        Variant   data[2] { { prop },  { CALI_TYPE_STRING, name.c_str(), name.size() } };

        node = sT->tree.get_path(2, &attr[0], &data[0], node);

        if (node) {
            // Check again if attribute already exists; might have been created by
            // another thread in the meantime.
            // We've created some redundant nodes then, but that's fine
            sG->attribute_lock.lock();

            auto it = sG->attribute_nodes.lower_bound(name);

            if (it == sG->attribute_nodes.end() || it->first != name) {
                sG->attribute_nodes.insert(it, std::make_pair(name, node));
                created_now = true;
            } else
                node = it->second;

            sG->attribute_lock.unlock();
        }
    }

    // Create attribute object

    Attribute attr = Attribute::make_attribute(node);

    if (created_now)
        for (auto& exp : sG->experiments)
            if (exp)
                exp->mP->events.create_attr_evt(this, exp.get(), attr);

    return attr;
}

/// \brief Find an attribute by name
///
/// While it should be signal safe, we do not recommend using this function in a signal handler.
///
/// \param name The attribute name
/// \return Attribute object, or Attribute::invalid if not found.

Attribute
Caliper::get_attribute(const std::string& name) const
{
    std::lock_guard<::siglock>
        g(sT->lock);

    Node* node = nullptr;

    sG->attribute_lock.lock();

    auto it = sG->attribute_nodes.find(name);

    if (it != sG->attribute_nodes.end())
        node = it->second;

    sG->attribute_lock.unlock();

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

    return Attribute::make_attribute(sT->tree.node(id));
}

/// \brief Get all attributes.
/// \note This function is _not_ signal safe.
/// \return   A vector that containing all attribute objects

std::vector<Attribute>
Caliper::get_attributes() const
{
    std::lock_guard<::siglock>
        g(sT->lock);
    std::lock_guard<std::mutex>
        g_a(sG->attribute_lock);

    std::vector<Attribute> ret;
    ret.reserve(sG->attribute_nodes.size());

    for (auto it : sG->attribute_nodes)
        ret.push_back(Attribute::make_attribute(it.second));

    return ret;
}

//
// --- Annotation dispatch API
//

/// Dispatch annotation region begin across all active experiments
void
Caliper::begin(const Attribute& attr, const Variant& data)
{
    for (auto& exp : sG->experiments)
        if (exp && exp->is_active())
            begin(exp.get(), attr, data);
}

/// Dispatch annotation region end across all active experiments
void
Caliper::end(const Attribute& attr)
{
    for (auto& exp : sG->experiments)
        if (exp && exp->is_active())
            end(exp.get(), attr);
}

/// Dispatch annotation region set across all active experiments
void
Caliper::set(const Attribute& attr, const Variant& data)
{
    for (auto& exp : sG->experiments)
        if (exp && exp->is_active())
            set(exp.get(), attr, data);
}

/// Dispatch memory region annotation across all active experiments
void
Caliper::memory_region_begin(const void* ptr, const char* label, size_t elem_size, size_t ndim, const size_t dims[])
{
    for (auto& exp : sG->experiments)
        if (exp && exp->is_active())
            memory_region_begin(exp.get(), ptr, label, elem_size, ndim, dims);
}

/// Dispatch memory region annotation end across all active experiments
void
Caliper::memory_region_end(const void* ptr)
{
    for (auto& exp : sG->experiments)
        if (exp && exp->is_active())
            memory_region_end(exp.get(), ptr);
}

///   Returns all entries with CALI_ATTR_GLOBAL set from the given experiment's
/// blackboard.

std::vector<Entry>
Caliper::get_globals(Experiment* exp)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    SnapshotRecord::FixedSnapshotRecord<80> rec_data;
    SnapshotRecord rec(rec_data);
    
    // All global attributes are process scope, so just grab the process blackboard
    exp->mP->blackboard.snapshot(&rec);

    std::vector<const Node*> nodes;

    SnapshotRecord::Data  data = rec.data();
    SnapshotRecord::Sizes size = rec.size();

    // Go through all process nodes and filter out the global entries
    for (size_t i = 0; i < size.n_nodes; ++i)
        for (const Node* node = data.node_entries[i]; node; node = node->parent())
            if (get_attribute(node->attribute()).properties() & CALI_ATTR_GLOBAL)
                nodes.push_back(node);

    // Restore original order
    std::reverse(nodes.begin(), nodes.end());

    std::vector<Entry> ret;

    if (!nodes.empty())
        ret.push_back(Entry(make_tree_entry(nodes.size(), nodes.data(), nullptr)));

    // Add potential AS_VALUE global entries
    for (size_t i = 0; i < size.n_immediate; ++i)
        if (get_attribute(data.immediate_attr[i]).properties() & CALI_ATTR_GLOBAL)
            ret.push_back(Entry(data.immediate_attr[i], data.immediate_data[i]));

    return ret;
}

std::vector<Entry>
Caliper::get_globals()
{
    return get_globals(sG->experiments.front().get());
}

// --- Snapshot interface

/// \brief Trigger and return a snapshot.
///
/// This function triggers a snapshot for a given experiment and returns a snapshot
/// record to the caller
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
Caliper::pull_snapshot(Experiment* exp, int scopes, const SnapshotRecord* trigger_info, SnapshotRecord* sbuf)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    // Save trigger info in snapshot buf

    if (trigger_info)
        sbuf->append(*trigger_info);

    // Invoke callbacks and get contextbuffer data

    exp->mP->events.snapshot(this, exp, scopes, trigger_info, sbuf);

    for (cali_context_scope_t s : { CALI_SCOPE_TASK, CALI_SCOPE_THREAD, CALI_SCOPE_PROCESS })
        if (scopes & s)
            exp->mP->get_blackboard(exp, s)->snapshot(sbuf);
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
Caliper::push_snapshot(Experiment* exp, int scopes, const SnapshotRecord* trigger_info)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    SnapshotRecord::FixedSnapshotRecord<80> snapshot_data;
    SnapshotRecord sbuf(snapshot_data);

    pull_snapshot(exp, scopes, trigger_info, &sbuf);

    exp->mP->events.process_snapshot(this, exp, trigger_info, &sbuf);
}


/// \brief Flush aggregation/trace buffer contents.
void
Caliper::flush(Experiment* exp, const SnapshotRecord* flush_info, SnapshotFlushFn proc_fn)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    exp->mP->events.pre_flush_evt(this, exp, flush_info);

    if (exp->mP->events.postprocess_snapshot.empty()) {
        exp->mP->events.flush_evt(this, exp, flush_info, proc_fn);
    } else {
        exp->mP->events.flush_evt(this, exp, flush_info,
                             [this,exp,flush_info,proc_fn](const SnapshotRecord* input_snapshot) {
                                 SnapshotRecord::FixedSnapshotRecord<80> data;
                                 SnapshotRecord snapshot(data);

                                 snapshot.append(*input_snapshot);

                                 exp->mP->events.postprocess_snapshot(this, exp, &snapshot);
                                 return proc_fn(&snapshot);
                             });
    }
}


/// Forward aggregation/trace buffer contents to output services.
///
/// Flushes trace buffers and / or the aggregation database in the trace and aggregation
/// services, respectively. This will 
/// forward all buffered snapshot records to output services, e.g., report and recorder.
///
/// This function will invoke the pre_flush, flush, and flush_finish callbacks.
/// \note This function is not signal safe.
///
/// \param input_flush_info User-provided flush context information. Currently unused.

void
Caliper::flush_and_write(Experiment* exp, const SnapshotRecord* input_flush_info)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    SnapshotRecord::FixedSnapshotRecord<80> snapshot_data;
    SnapshotRecord flush_info(snapshot_data);

    if (input_flush_info)
        flush_info.append(*input_flush_info);

    exp->mP->get_blackboard(exp, CALI_SCOPE_PROCESS)->snapshot(&flush_info);
    exp->mP->get_blackboard(exp, CALI_SCOPE_THREAD )->snapshot(&flush_info);

    Log(1).stream() << "Flushing Caliper data" << std::endl;

    exp->mP->events.pre_write_evt(this, exp, &flush_info);

    flush(exp, &flush_info,
          [this,exp,&flush_info](const SnapshotRecord* snapshot){
              exp->mP->events.write_snapshot(this, exp, &flush_info, snapshot);
              return true;
          });

    exp->mP->events.post_write_evt(this, exp, &flush_info);
}


/// Clear aggregation and/or trace buffers.
///
/// Clears aggregation and trace buffers. Data in those buffers
/// that has not been written yet will be lost.
///
/// \note This function is not signal safe.

void
Caliper::clear(Experiment* exp)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    exp->mP->events.clear_evt(this, exp);
}


// --- Annotation interface

/// \brief Push attribute:value pair on blackboard of the given experiment.
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
/// \param exp  Experiment
/// \param attr Attribute key
/// \param data Value to set

cali_err
Caliper::begin(Experiment* exp, const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_EINV;

    if (!exp || attr == Attribute::invalid)
        return CALI_EINV;

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!attr.skip_events())
        exp->mP->events.pre_begin_evt(this, exp, attr, data);

    Attribute key = exp->mP->get_key(attr, sG->key_attr);
    ContextBuffer* sb = exp->mP->get_blackboard(exp, attr);

    if (attr.store_as_value())
        ret = sb->set(attr, data);
    else
        ret = sb->set_node(key,
                           sT->tree.get_path(1, &attr, &data,
                                             sb->get_node(key)));

    // invoke callbacks
    if (!attr.skip_events())
        exp->mP->events.post_begin_evt(this, exp, attr, data);

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
Caliper::end(Experiment* exp, const Attribute& attr)
{
    cali_err ret = CALI_EINV;

    ContextBuffer* sb = exp->mP->get_blackboard(exp, attr);        
    Entry e = get(exp, attr);

    if (e.is_empty())
        return CALI_ESTACK;

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!attr.skip_events())
        if (!e.is_empty()) // prevent executing events for
            exp->mP->events.pre_end_evt(this, exp, attr, e.value());

    Attribute key = exp->mP->get_key(attr, sG->key_attr);

    if (attr.store_as_value())
        ret = sb->unset(attr);
    else {
        Node* node = sb->get_node(key);

        if (node) {
            node = sT->tree.remove_first_in_path(node, attr);

            if (node == sT->tree.root())
                ret = sb->unset(key);
            else if (node)
                ret = sb->set_node(key, node);
        }

        if (!node)
            Log(0).stream() << "error: trying to end inactive attribute " << attr.name() << endl;
    }

    // invoke callbacks
    if (!attr.skip_events())
        exp->mP->events.post_end_evt(this, exp, attr, e.value());

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
Caliper::set(Experiment* exp, const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_EINV;

    if (attr == Attribute::invalid)
        return CALI_EINV;

    std::lock_guard<::siglock>
        g(sT->lock);

    ContextBuffer* sb = exp->mP->get_blackboard(exp, attr);
    Attribute key = exp->mP->get_key(attr, sG->key_attr);

    // invoke callbacks
    if (!attr.skip_events())
        exp->mP->events.pre_set_evt(this, exp, attr, data);

    if (attr.store_as_value())
        ret = sb->set(attr, data);
    else {
        ret = sb->set_node(key, sT->tree.replace_first_in_path(sb->get_node(key), attr, data));
    }

    // invoke callbacks
    if (!attr.skip_events())
        exp->mP->events.post_set_evt(this, exp, attr, data);

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
Caliper::set_path(Experiment* exp, const Attribute& attr, size_t n, const Variant* data) {
    cali_err ret = CALI_EINV;

    if (n < 1)
        return CALI_SUCCESS;
    if (attr == Attribute::invalid)
        return CALI_EINV;

    std::lock_guard<::siglock>
        g(sT->lock);
    
    ContextBuffer* sb = exp->mP->get_blackboard(exp, attr);    

    // invoke callbacks
    if (!attr.skip_events())
        exp->mP->events.pre_set_evt(this, exp, attr, data[n-1]);

    if (attr.store_as_value()) {
        Log(0).stream() << "error: set_path() invoked with immediate-value attribute " << attr.name() << endl;
        ret = CALI_EINV;
    } else {
        Attribute key = exp->mP->get_key(attr, sG->key_attr);

        ret = sb->set_node(key,
                           sT->tree.replace_all_in_path(sb->get_node(key), attr, n, data));
    }

    // invoke callbacks
    if (!attr.skip_events())
        exp->mP->events.post_set_evt(this, exp, attr, data[n-1]);

    return ret;
}

// --- Query

/// \brief Retrieve top-most entry for the given attribute key from the blackboard
///    of the given experiment.
///
/// This function is signal safe.
///
/// \param attr Attribute key.
///
/// \return The top-most entry on the blackboard for the given attribute key.
///         An empty Entry object if this attribute is not set.

Entry
Caliper::get(Experiment* exp, const Attribute& attr)
{
    Entry e {  Entry::empty };

    if (attr == Attribute::invalid)
        return Entry::empty;

    std::lock_guard<::siglock>
        g(sT->lock);

    ContextBuffer* sb = exp->mP->get_blackboard(exp, attr);

    if (attr.store_as_value())
        return Entry(attr, sb->get(attr));
    else
        return Entry(sT->tree.find_node_with_attribute(attr, sb->get_node(exp->mP->get_key(attr, sG->key_attr))));

    return e;
}

// --- Memory region tracking

void
Caliper::memory_region_begin(Experiment* exp, const void* ptr, const char* label, size_t elem_size, size_t ndims, const size_t dims[])
{
    std::lock_guard<::siglock>
        g(sT->lock);

    exp->mP->events.track_mem_evt(this, exp, ptr, label, elem_size, ndims, dims);    
}

void
Caliper::memory_region_end(Experiment* exp, const void* ptr)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    exp->mP->events.untrack_mem_evt(this, exp, ptr);
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
/// \param parent (Optional) parent node for any treee elements.
void
Caliper::make_entrylist(size_t n, const Attribute* attr, const Variant* value, SnapshotRecord& list, Node* parent)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    Node* node = parent;

    for (size_t i = 0; i < n; ++i)
        if (attr[i].store_as_value())
            list.append(attr[i].id(), value[i]);
        else
            node = sT->tree.get_path(1, &attr[i], &value[i], node);

    if (node && node != parent)
        list.append(node);
}

/// \brief Create a snapshot record (entry list) from the given attribute and
///   list of values
///
/// This function is signal-safe.
///
/// \param attr   Attribute list
/// \param n      Number of elements in attribute/value lists
/// \param value  Value list
/// \param list   Output record. Must be large enough to hold all entries.

void
Caliper::make_entrylist(const Attribute& attr, size_t n, const Variant* value, SnapshotRecord& list)
{
    if (n < 1)
        return;

    std::lock_guard<::siglock>
        g(sT->lock);

    if (attr.store_as_value())
        // only store one value entry
        list.append(attr.id(), value[0]);
    else
        list.append(sT->tree.get_path(attr, n, value, nullptr));
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
        g(sT->lock);

    return sT->tree.get_path(n, nodelist, parent);
}

/// \brief Return a context tree path for the given key:value pairs.
///
/// \note This function is signal safe.
///
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
        g(sT->lock);

    return sT->tree.get_path(1, &attr, &data, parent);
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
    return sT->tree.node(id);
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
Caliper::exchange(Experiment* exp, const Attribute& attr, const Variant& data)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    return exp->mP->get_blackboard(exp, attr)->exchange(attr, data);
}

//
// --- Experiment API
//

Experiment*
Caliper::create_experiment(const char* name, const RuntimeConfig& cfg)
{
    Experiment* exp = new Experiment(sG->experiments.size(), name, cfg);
    sG->experiments.emplace_back(exp);            

    Caliper c(false);

    // Create and set key & version attributes

    c.set(exp, c.create_attribute("cali.caliper.version", CALI_TYPE_STRING,
                                  CALI_ATTR_SKIP_EVENTS | CALI_ATTR_GLOBAL),
          Variant(CALI_TYPE_STRING, CALIPER_VERSION, sizeof(CALIPER_VERSION)));
    c.set(exp, c.create_attribute("cali.experiment", CALI_TYPE_STRING,
                                  CALI_ATTR_SKIP_EVENTS | CALI_ATTR_GLOBAL),
          Variant(CALI_TYPE_STRING, name, strlen(name)+1));
        
    Services::register_services(&c, exp);

    Log(1).stream() << "Creating experiment \"" << name << "\"" << std::endl;

    if (exp->config().get("caliper", "config_check").to_bool())
        config_sanity_check(exp->config());
    if (Log::verbosity() >= 3)
        exp->config().print( Log(3).stream() << "Configuration:\n" );

    exp->mP->events.post_init_evt(&c, exp);

    return exp;
}

Experiment*
Caliper::get_experiment(cali_id_t id)
{
    if (sG->experiments.size() <= id)
        return nullptr;

    return sG->experiments[id].get();
}

std::vector<Experiment*>
Caliper::get_experiments()
{
    std::vector<Experiment*> ret;

    ret.reserve(sG->experiments.size());

    for (auto &exp : sG->experiments)
        if (exp)
            ret.push_back(exp.get());

    return ret;
}

//
// --- Caliper constructor & singleton API
//

/// \brief Construct a Caliper instance object.
/// \see instance()

Caliper::Caliper()
    : m_is_signal(false)
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
            return Caliper(true);

        lock_guard<mutex> lock(GlobalData::s_init_mutex);

        if (!sG) {
            if (atexit(&Caliper::release) != 0)
                Log(0).stream() << "Unable to register exit handler";
            
            sT.reset(new Caliper::ThreadData);
            sG.reset(new Caliper::GlobalData);

            sG->init();
            
            GlobalData::s_init_lock = 0;
        }
    }

    if (!sT)
        sT.reset(new ThreadData);
    
    {
        using expI = Experiment::ExperimentImpl;
        
        if (!expI::sT)
            expI::sT.reset(new expI::ThreadData(sG->experiments.size()));
    }
    
    return Caliper(false);
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
    return Caliper(true);
}

Caliper::operator bool() const
{
    return (sG && sT && !(m_is_signal && sT->lock.is_locked()));
}

void
Caliper::release()
{
    Caliper c;

    if (c) {
        for (auto &exp : sG->experiments) {
            if (exp->mP->flush_on_exit) 
                c.flush_and_write(exp.get(), nullptr);

            c.clear(exp.get());
            exp->mP->events.finish_evt(&c, exp.get());
        }
    }

    // Don't delete global data, some thread-specific finalization may occur after this point
    // sG.reset(nullptr);
}

/// \brief Test if Caliper has been initialized yet.

bool
Caliper::is_initialized()
{
    return sG && sT;
}

/// \brief Add a list of available caliper services.
/// Adds services that will be made available by %Caliper. This does *not*
/// activate the services automatically, they must still be listed in the
/// CALI_SERVICES_ENABLE configuration variable at runtime.
/// Services must be provided in a list of CaliperService entries,
/// terminated by a `{ nullptr, nullptr}` entry. Example:
///
/// \code
///   extern void myservice_register(Caliper* c);
///
///   CaliperService my_services[] = {
///     { "myservice", myservice_register },
///     { nullptr, nullptr }
///   };
///
///   Caliper::add_services(my_services);
/// \endcode
///
/// This is only effective _before_ %Caliper is initialized, and should be
/// called e.g. during static initialization or from a library constructor.

void
Caliper::add_services(const CaliperService* s)
{
    if (is_initialized())
        Log(0).stream() << "add_services(): Caliper is already initialized - cannot add new services" << std::endl;
    else    
        Services::add_services(s);
}

void
Caliper::add_init_hook(void (*hook)())
{
    if (is_initialized())
        Log(0).stream() << "add_init_hook(): Caliper is already initialized - cannot add init hook" << std::endl;
    else
        sG->add_init_hook(hook);
}

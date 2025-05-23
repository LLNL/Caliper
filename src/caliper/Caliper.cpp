// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Caliper.cpp
/// Caliper main class
///

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "Blackboard.h"
#include "MetadataTree.h"

#include "caliper/common/Node.h"
#include "caliper/common/Log.h"

#include "../common/RuntimeConfig.h"

#include "../services/Services.h"

#include <signal.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#define SNAP_MAX 120

using namespace cali;
using namespace cali::internal;

using SnapshotRecord = FixedSizeSnapshotRecord<SNAP_MAX>;

extern cali_id_t cali_class_aggregatable_attr_id;

namespace cali
{

extern void init_attribute_classes(Caliper* c);
extern void init_api_attributes(Caliper* c);

extern void init_submodules();

extern void config_sanity_check(const char*, RuntimeConfig);

namespace internal
{

extern void init_builtin_configmanager(Caliper* c);

}

} // namespace cali

namespace
{

// --- Siglock

class siglock
{
    volatile sig_atomic_t m_lock;

public:

    siglock() : m_lock(0) {}

    inline void lock() { ++m_lock; }
    inline void unlock() { --m_lock; }

    inline bool is_locked() const { return (m_lock > 0); }
};

// --- helper functions

void log_invalid_cfg_value(const char* var, const char* value, const char* prefix = nullptr)
{
    Log(0).stream() << (prefix ? std::string(prefix) + ": " : std::string("")) << "Invalid value \"" << value
                    << "\" for " << var << std::endl;
}

std::ostream& print_available_services(std::ostream& os)
{
    std::vector<std::string> services = services::get_available_services();

    int count = 0;
    for (const std::string& s : services)
        os << (count++ > 0 ? "," : "") << s;

    if (!count)
        os << "none";

    return os;
}

std::vector<Entry> get_globals_from_blackboard(Caliper* c, const Blackboard& blackboard, std::mutex& blackboard_lock)
{
    FixedSizeSnapshotRecord<SNAP_MAX> rec;
    
    {
        std::lock_guard<std::mutex> g(blackboard_lock);
        blackboard.snapshot(rec.builder());
    }

    std::vector<Entry>       ret;
    std::vector<const Node*> nodes;

    for (const Entry& e : rec.view()) {
        if (e.is_reference()) {
            for (const Node* node = e.node(); node; node = node->parent())
                if (c->get_attribute(node->attribute()).is_global())
                    nodes.push_back(node);
        } else if (c->get_attribute(e.attribute()).is_global())
            ret.push_back(e);
    }

    // Restore original order
    std::reverse(nodes.begin(), nodes.end());

    if (!nodes.empty())
        ret.push_back(Entry(c->make_tree_entry(nodes.size(), nodes.data(), nullptr)));

    return ret;
}

void make_default_channel()
{
    //   Creates default channel (which reads env vars and/or caliper.config)
    // and initializes builtin ConfigManager during initialization
    RuntimeConfig                            cfg = RuntimeConfig::get_default_config();
    const RuntimeConfig::config_entry_list_t configdata { { "enable", "" } };
    std::vector<std::string> services = cfg.init("services", configdata).get("enable").to_stringlist(",:");

    Caliper c;

    if (services.empty())
        Log(1).stream() << "No manual config specified, disabling default channel\n";
    else {
        auto channel = c.create_channel("default", cfg);
        c.activate_channel(channel);
    }

    internal::init_builtin_configmanager(&c);
}

} // namespace

//
// Caliper channel data
//

struct cali::ChannelBody {
    static const ConfigSet::Entry s_configdata[];

    cali_id_t   id;
    std::string name;
    bool        is_active;

    RuntimeConfig   config;
    Channel::Events events; ///< callbacks

    bool flush_on_exit;

    Blackboard channel_blackboard;
    std::mutex channel_blackboard_lock;

    ChannelBody(cali_id_t _id, const char* _name, const RuntimeConfig& cfg)
        : id(_id), name(_name), is_active(false), config(cfg)
    {
        ConfigSet cali_cfg = config.init("channel", s_configdata);
        flush_on_exit = cali_cfg.get("flush_on_exit").to_bool();
    }

    ~ChannelBody()
    {
        if (Log::verbosity() >= 2) {
            channel_blackboard.print_statistics(Log(2).stream() << name << " channel blackboard: ") << std::endl;
        }
    }
};

const ConfigSet::Entry cali::ChannelBody::s_configdata[] = {
    // key, type, value, short description, long description
    { "config_check",
      CALI_TYPE_BOOL,
      "true",
      "Perform configuration sanity check at initialization",
      "Perform configuration sanity check at initialization" },
    { "flush_on_exit",
      CALI_TYPE_BOOL,
      "true",
      "Flush Caliper buffers at program exit",
      "Flush Caliper buffers at program exit" },
    ConfigSet::Terminator
};

Channel::Channel(cali_id_t id, const char* name, const RuntimeConfig& cfg) : mP(new ChannelBody(id, name, cfg))
{}

Channel::~Channel()
{}

Channel::Events& Channel::events()
{
    return mP->events;
}

RuntimeConfig Channel::config()
{
    return mP->config;
}

std::string Channel::name() const
{
    return mP->name;
}

bool Channel::is_active() const
{
    return mP && mP->is_active;
}

cali_id_t Channel::id() const
{
    return mP->id;
}

//
// --- Caliper thread data
//

/// \brief Per-thread data for the Caliper object
struct Caliper::ThreadData {
    MetadataTree tree;
    ::siglock    lock;

    SnapshotRecord snapshot;

    // This thread's blackboard
    Blackboard thread_blackboard;
    // copy of the last process blackboard snapshot
    SnapshotRecord process_snapshot;
    // version of the last process blackboard snapshot
    int process_bb_count;

    bool is_initial_thread;
    bool stack_error;

    ThreadData(bool initial_thread = false)
        : process_bb_count(-1), is_initial_thread(initial_thread), stack_error(false)
    {}

    ~ThreadData()
    {
        if (Log::verbosity() >= 2)
            print_detailed_stats(Log(2).stream());
    }

    void print_detailed_stats(std::ostream& os)
    {
        tree.print_statistics(os << "Releasing Caliper thread data: \n") << std::endl;
        thread_blackboard.print_statistics(os << "  Thread blackboard: ") << std::endl;
    }

    inline void update_process_snapshot(Blackboard& process_blackboard, std::mutex& process_blackboard_lock)
    {
        //   Check if the process or channel blackboards have been updated
        // since the last snapshot on this thread.
        //   We keep a copy of the last process/channel snapshot data in our
        // thread-local storage so we don't have to access (and lock) the
        // process/channel blackboards when they haven't changed.
        int process_bb_count_now = process_blackboard.count();
        if (process_bb_count_now > process_bb_count) {
            //   Process blackboard has been updated:
            // update thread-local snapshot data
            process_snapshot.reset();
            std::lock_guard<std::mutex> g(process_blackboard_lock);
            process_blackboard.snapshot(process_snapshot.builder());
            process_bb_count = process_bb_count_now;
        }
    }
};

//
// --- Caliper Global Data
//

struct Caliper::GlobalData {
    // --- static data

    static volatile sig_atomic_t s_init_lock;
    static std::mutex            s_init_mutex;

    static const ConfigSet::Entry s_configdata[];

    // --- data

    mutable std::mutex               attribute_lock;
    std::map<std::string, Attribute> attribute_map;

    std::map<std::string, int> attribute_prop_presets;
    int                        attribute_default_scope;

    Blackboard process_blackboard;
    std::mutex process_blackboard_lock;

    std::vector<Channel> all_channels;
    std::vector<Channel> active_channels;

    size_t max_active_channels;

    std::vector<ThreadData*> thread_data;
    std::mutex               thread_data_lock;

    // --- constructor

    GlobalData(ThreadData* sT) : attribute_default_scope { CALI_ATTR_SCOPE_THREAD }, max_active_channels { 0 }
    {
        // put the attribute [name,type,prop] attributes in the map

        Attribute name_attr = Attribute::make_attribute(sT->tree.node(Attribute::NAME_ATTR_ID));
        Attribute type_attr = Attribute::make_attribute(sT->tree.node(Attribute::TYPE_ATTR_ID));
        Attribute prop_attr = Attribute::make_attribute(sT->tree.node(Attribute::PROP_ATTR_ID));

        attribute_map.insert(make_pair(name_attr.name(), name_attr));
        attribute_map.insert(make_pair(prop_attr.name(), prop_attr));
        attribute_map.insert(make_pair(type_attr.name(), type_attr));
    }

    ~GlobalData()
    {
        // prevent re-initialization
        s_init_lock = 2;

        if (Log::verbosity() >= 2) {
            Log(2).stream() << "Releasing Caliper global data.\n"
                            << "  Max active channels: " << max_active_channels << std::endl;
            process_blackboard.print_statistics(Log(2).stream() << "Process blackboard: ") << std::endl;
        }

        {
            std::lock_guard<std::mutex> g(thread_data_lock);

            std::for_each(thread_data.begin(), thread_data.end(), [](ThreadData* d) { delete d; });
            thread_data.clear();
        }

        gObj.g_ptr = nullptr;

        MetadataTree::release();

        Log(1).stream() << "Finished" << std::endl;
        Log::fini();
    }

    void parse_attribute_config(const ConfigSet& config)
    {
        auto preset_cfg = config.get("attribute_properties").to_stringlist();

        for (const std::string& s : preset_cfg) {
            auto p = s.find_first_of('=');

            if (p == std::string::npos)
                continue;

            int prop = cali_string2prop(s.substr(p + 1).c_str());

            attribute_prop_presets.insert(make_pair(s.substr(0, p), prop));
        }

        std::string scope_str = config.get("attribute_default_scope").to_string();

        if (scope_str == "process")
            attribute_default_scope = CALI_ATTR_SCOPE_PROCESS;
        else if (scope_str == "thread")
            attribute_default_scope = CALI_ATTR_SCOPE_THREAD;
        else
            log_invalid_cfg_value("CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE", scope_str.c_str());
    }

    void init()
    {
        init_submodules();

        parse_attribute_config(RuntimeConfig::get_default_config().init("caliper", s_configdata));

        if (Log::verbosity() >= 2)
            print_available_services(Log(2).stream() << "Available services: ") << std::endl;

        Caliper c(this, tObj.t_ptr, false);

        init_attribute_classes(&c);
        init_api_attributes(&c);

        c.set(
            c.create_attribute("cali.caliper.version", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_GLOBAL),
            Variant(CALIPER_VERSION)
        );

        Log(1).stream() << "Initialized" << std::endl;
    }

    ThreadData* add_thread_data(ThreadData* t)
    {
        tObj.t_ptr = t;

        std::lock_guard<std::mutex> g(thread_data_lock);

        thread_data.push_back(t);
        return t;
    }

    //   S_TLSObject uses C++ thread-local storage to hold a pointer to the
    // thread-local data. This is for lookup only, GlobalData owns all
    // ThreadData objects. The object also notifies us of thread destruction
    // via its destructor.
    //   In addition, the destructor for the initial thread object serves as
    // finalization trigger on program exit, together with S_GObject below.
    // The destruction order for thread-local and regular static objects is
    // undefined, so whoever is destroyed first triggers finalization.
    struct S_TLSObject {
        ThreadData* t_ptr;

        S_TLSObject() : t_ptr(nullptr) {}

        ~S_TLSObject()
        {
            // Only use if we're still active
            if (t_ptr && s_init_lock == 0) {
                Caliper c(gObj.g_ptr, t_ptr, false);

                if (t_ptr->is_initial_thread) {
                    c.finalize();
                    delete gObj.g_ptr;
                } else {
                    c.release_thread();
                }
            }

            t_ptr = nullptr;
        }
    };

    //   S_GObject holds a pointer to our global data. Also, its destructor
    // serves as finalization trigger, together with S_TLSObject above.
    struct S_GObject {
        GlobalData* g_ptr;

        // No constructor: Could overwrite existing Caliper object if Caliper
        // is initialized somewhere before this static constructor runs.
        // S_GObject()
        //     : g_ptr(nullptr)
        //     { }

        ~S_GObject()
        {
            // Only use if we're still active
            if (g_ptr && s_init_lock == 0) {
                Caliper c(g_ptr, tObj.t_ptr, false);

                c.finalize();
                delete g_ptr;
            }

            g_ptr = nullptr;
        }
    };

    static S_GObject                gObj;
    static thread_local S_TLSObject tObj;
};

// --- static member initialization

volatile sig_atomic_t Caliper::GlobalData::s_init_lock = 1;
std::mutex            Caliper::GlobalData::s_init_mutex;

Caliper::GlobalData::S_GObject                Caliper::GlobalData::gObj;
thread_local Caliper::GlobalData::S_TLSObject Caliper::GlobalData::tObj;

const ConfigSet::Entry Caliper::GlobalData::s_configdata[] = {
    // key, type, value, short description, long description
    { "attribute_properties",
      CALI_TYPE_STRING,
      "",
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
      "  nested:        Values are properly nested with the call stack and other nested attributes\n" },
    { "attribute_default_scope",
      CALI_TYPE_STRING,
      "thread",
      "Default scope for attributes",
      "Default scope for attributes. Possible values are\n"
      "  process:   Process scope\n"
      "  thread:    Thread scope" },

    ConfigSet::Terminator
};

namespace
{

constexpr cali_id_t REGION_KEY { 1 };
constexpr cali_id_t UNALIGNED_KEY { 2 };

// Get the blackboard key, which determines the blackboard slot for each
// attribute. By default, we merge most attributes into two slots:
// region_key for region-type (begin/end) attributes, unaligned_key for
// set-type attributes. We skip stack nesting checks for unaligned
// attributes. Immediate (as_value) and nomerge attributes get
// their own slots.
inline cali_id_t get_blackboard_key(cali_id_t attr_id, int prop)
{
    if (prop & CALI_ATTR_ASVALUE)
        return attr_id;
    if (prop & CALI_ATTR_UNALIGNED)
        return UNALIGNED_KEY;

    return REGION_KEY;
}

inline cali_id_t get_blackboard_key_for_reference_entry(int prop)
{
    return prop & CALI_ATTR_UNALIGNED ? UNALIGNED_KEY : REGION_KEY;
}

void log_stack_error(const Node* stack, const Attribute& attr)
{
    std::string stackstr;
    std::string helpstr;

    if (stack) {
        stackstr = " but current region is \"";
        stackstr.append(stack->data().to_string());
        stackstr.append("\".");
    } else
        stackstr = " but region stack is empty!";

    Log(0).stream() << "Region stack mismatch: Trying to end \"" << attr.name() << "\"" << stackstr
                    << "\n  Ceasing region tracking!" << helpstr << std::endl;
}

void log_stack_value_error(const Entry& current, Attribute attr, const Variant& expect)
{
    std::string error;
    if (current.empty())
        error = "stack is empty";
    else {
        error = "current value is ";
        error.append(attr.name());
        error.append("=");
        error.append(current.value().to_string());
    }

    Log(0).stream() << "Stack value mismatch: Trying to end " << attr.name() << "=" << expect.to_string() << " but "
                    << error << std::endl;
}

struct BlackboardEntry {
    Entry merged_entry;
    Entry entry;
};

inline BlackboardEntry load_current_entry(const Attribute& attr, cali_id_t key, Blackboard& blackboard)
{
    Entry merged_entry = blackboard.get(key);
    Entry entry        = merged_entry.get(attr);

    if (merged_entry.attribute() != attr.id()) {
        if (entry.empty()) {
            log_stack_error(nullptr, attr);
            return { Entry(), Entry() };
        }
        if (key != UNALIGNED_KEY) {
            log_stack_error(merged_entry.node(), attr);
            return { Entry(), Entry() };
        }
    }

    return { merged_entry, entry };
}

inline void handle_begin(
    const Attribute& attr,
    const Variant&   value,
    int              prop,
    Blackboard&      blackboard,
    MetadataTree&    tree
)
{
    if (prop & CALI_ATTR_ASVALUE) {
        blackboard.set(attr.id(), Entry(attr, value), !(prop & CALI_ATTR_HIDDEN));
    } else {
        cali_id_t key   = get_blackboard_key_for_reference_entry(prop);
        Entry     entry = Entry(tree.get_child(attr, value, blackboard.get(key).node()));
        blackboard.set(key, entry, !(prop & CALI_ATTR_HIDDEN));
    }
}

inline void handle_end(
    const Attribute&       attr,
    int                    prop,
    const BlackboardEntry& current,
    cali_id_t              key,
    Blackboard&            blackboard,
    MetadataTree&          tree
)
{
    if (prop & CALI_ATTR_ASVALUE)
        blackboard.del(key);
    else {
        Node* node = current.merged_entry.node()->parent();

        if (node == tree.root())
            blackboard.del(key);
        else {
            if (current.merged_entry.node() != current.entry.node())
                node = tree.remove_first_in_path(current.merged_entry.node(), attr);

            blackboard.set(key, Entry(node), !(prop & CALI_ATTR_HIDDEN));
        }
    }
}

inline void handle_set(
    const Attribute& attr,
    const Variant&   value,
    int              prop,
    Blackboard&      blackboard,
    MetadataTree&    tree
)
{
    if (prop & CALI_ATTR_ASVALUE)
        blackboard.set(attr.id(), Entry(attr, value), !(prop & CALI_ATTR_HIDDEN));
    else {
        cali_id_t key  = get_blackboard_key_for_reference_entry(prop);
        Node*     node = blackboard.get(key).node();
        blackboard.set(key, tree.replace_first_in_path(node, attr, value), !(prop & CALI_ATTR_HIDDEN));
    }
}

} // namespace

//
// Caliper class definition
//

// --- Attribute interface

Attribute Caliper::create_attribute(
    const std::string& name,
    cali_attr_type     type,
    int                prop,
    int                n_meta,
    const Attribute*   meta_attr,
    const Variant*     meta_val
)
{
    assert(sG != 0);

    std::lock_guard<::siglock> g(sT->lock);

    // Check if an attribute with this name already exists
    {
        std::lock_guard<std::mutex> ga(sG->attribute_lock);

        auto it = sG->attribute_map.find(name);
        if (it != sG->attribute_map.end())
            return it->second;
    }

    Node* node = nullptr;

    // Get type node
    node = sT->tree.type_node(type);

    // Add metadata nodes.
    for (int n = 0; n < n_meta; ++n)
        node = sT->tree.get_child(meta_attr[n], meta_val[n], node);

    // Look for attribute properties in presets
    auto propit = sG->attribute_prop_presets.find(name);
    if (propit != sG->attribute_prop_presets.end())
        prop = propit->second;

    // Set scope to PROCESS for all global attributes and mark them as unaligned
    if (prop & CALI_ATTR_GLOBAL) {
        prop &= ~CALI_ATTR_SCOPE_MASK;
        prop |= CALI_ATTR_SCOPE_PROCESS;
        prop |= CALI_ATTR_UNALIGNED;
    }
    // Set scope to default scope if none is set
    if ((prop & CALI_ATTR_SCOPE_MASK) == 0)
        prop |= sG->attribute_default_scope;

    // Set CALI_ATTR_AGGREGATABLE property if class.aggregatable metadata is set
    for (const Node* tmp = node; tmp; tmp = tmp->parent())
        if (tmp->attribute() == cali_class_aggregatable_attr_id && tmp->data() == Variant(true)) {
            prop |= CALI_ATTR_AGGREGATABLE;
            break;
        }

    Attribute name_attr = Attribute::make_attribute(sT->tree.node(Attribute::NAME_ATTR_ID));
    Attribute prop_attr = Attribute::make_attribute(sT->tree.node(Attribute::PROP_ATTR_ID));

    node = sT->tree.get_child(prop_attr, Variant(prop), node);
    node = sT->tree.get_child(name_attr, Variant(CALI_TYPE_STRING, name.data(), name.size()), node);

    {
        // Check again if attribute already exists; might have been created by
        // another thread in the meantime.
        // We've created some redundant nodes then, but that's fine

        std::lock_guard<std::mutex> ga(sG->attribute_lock);

        auto it = sG->attribute_map.lower_bound(name);

        if (it == sG->attribute_map.end() || it->first != name)
            sG->attribute_map.insert(it, std::make_pair(name, Attribute::make_attribute(node)));
        else
            return it->second;
    }

    // Create attribute object

    Attribute attr = Attribute::make_attribute(node);

    for (auto& channel : sG->all_channels)
        channel.mP->events.create_attr_evt(this, attr);

    return attr;
}

Attribute Caliper::get_attribute(const std::string& name) const
{
    std::lock_guard<::siglock>  gs(sT->lock);
    std::lock_guard<std::mutex> ga(sG->attribute_lock);

    auto it = sG->attribute_map.find(name);

    return it != sG->attribute_map.end() ? it->second : Attribute();
}

Attribute Caliper::get_attribute(cali_id_t id) const
{
    // no signal lock necessary
    return Attribute::make_attribute(sT->tree.node(id));
}

std::vector<Attribute> Caliper::get_all_attributes() const
{
    std::lock_guard<::siglock>  g(sT->lock);
    std::lock_guard<std::mutex> g_a(sG->attribute_lock);

    std::vector<Attribute> ret;
    ret.reserve(sG->attribute_map.size());

    for (auto it : sG->attribute_map)
        ret.push_back(it.second);

    return ret;
}

//
// --- Annotation dispatch API
//

/// Dispatch memory region annotation across all active channels
void Caliper::memory_region_begin(
    const void*      ptr,
    const char*      label,
    size_t           elem_size,
    size_t           ndim,
    const size_t     dims[],
    size_t           nextra,
    const Attribute* extra_attrs,
    const Variant*   extra_vals
)
{
    for (auto& channel : sG->active_channels)
        memory_region_begin(channel.body(), ptr, label, elem_size, ndim, dims, nextra, extra_attrs, extra_vals);
}

/// Dispatch memory region annotation end across all active channels
void Caliper::memory_region_end(const void* ptr)
{
    for (auto& channel : sG->active_channels)
        memory_region_end(channel.body(), ptr);
}

///   Returns all entries with CALI_ATTR_GLOBAL set from the process
/// blackboard.
std::vector<Entry> Caliper::get_globals()
{
    std::lock_guard<::siglock> g(sT->lock);

    return get_globals_from_blackboard(this, sG->process_blackboard, sG->process_blackboard_lock);
}

///   Returns all entries with CALI_ATTR_GLOBAL set from the given channel's
/// and the process blackboard.
std::vector<Entry> Caliper::get_globals(ChannelBody* chB)
{
    std::lock_guard<::siglock> g(sT->lock);

    std::vector<Entry> ret = get_globals_from_blackboard(this, sG->process_blackboard, sG->process_blackboard_lock);
    std::vector<Entry> tmp = get_globals_from_blackboard(this, chB->channel_blackboard, chB->channel_blackboard_lock);

    ret.insert(ret.end(), tmp.begin(), tmp.end());

    return ret;
}

// --- Snapshot interface

void Caliper::pull_context(SnapshotBuilder& rec)
{
    std::lock_guard<::siglock> g(sT->lock);

    // Get thread blackboard data
    sT->thread_blackboard.snapshot(rec);

    // Get process blackboard data
    sT->update_process_snapshot(sG->process_blackboard, sG->process_blackboard_lock);
    rec.append(sT->process_snapshot.view());
}

void Caliper::pull_snapshot(ChannelBody* chB, SnapshotView trigger_info, SnapshotBuilder& rec)
{
    std::lock_guard<::siglock> g(sT->lock);

    rec.append(trigger_info);
    chB->events.snapshot(this, trigger_info, rec);

    sT->thread_blackboard.snapshot(rec);
    sT->update_process_snapshot(sG->process_blackboard, sG->process_blackboard_lock);
    rec.append(sT->process_snapshot.view());
}

void Caliper::push_snapshot(ChannelBody* chB, SnapshotView trigger_info)
{
    std::lock_guard<::siglock> g(sT->lock);

    sT->snapshot.reset();
    SnapshotBuilder& rec = sT->snapshot.builder();

    sT->thread_blackboard.snapshot(rec);
    sT->update_process_snapshot(sG->process_blackboard, sG->process_blackboard_lock);
    rec.append(sT->process_snapshot.view());

    rec.append(trigger_info);

    chB->events.snapshot(this, trigger_info, rec);
    chB->events.process_snapshot(this, trigger_info, rec.view());
}

void Caliper::push_snapshot_replace(ChannelBody* chB, SnapshotView trigger_info, const Entry& target)
{
    std::lock_guard<::siglock> g(sT->lock);

    sT->snapshot.reset();
    SnapshotBuilder& rec = sT->snapshot.builder();

    sT->thread_blackboard.snapshot(rec);
    sT->update_process_snapshot(sG->process_blackboard, sG->process_blackboard_lock);
    rec.append(sT->process_snapshot.view());

    // remove/replace target entry from blackboard snapshot
    rec.remove(target);
    rec.append(trigger_info);

    chB->events.snapshot(this, trigger_info, rec);
    chB->events.process_snapshot(this, trigger_info, rec.view());
}

void Caliper::flush(ChannelBody* chB, SnapshotView flush_info, SnapshotFlushFn proc_fn)
{
    std::lock_guard<::siglock> g(sT->lock);

    chB->events.pre_flush_evt(this, chB, flush_info);

    if (chB->events.postprocess_snapshot.empty()) {
        chB->events.flush_evt(this, flush_info, proc_fn);
    } else {
        chB->events.flush_evt(
            this,
            flush_info,
            [this, chB, proc_fn](CaliperMetadataAccessInterface&, const std::vector<Entry>& rec) {
                std::vector<Entry> mrec(rec);
                chB->events.postprocess_snapshot(this, mrec);
                proc_fn(*this, mrec);
            }
        );
    }

    chB->events.post_flush_evt(this, chB, flush_info);
}

void Caliper::flush_and_write(ChannelBody* chB, SnapshotView input_flush_info)
{
    std::lock_guard<::siglock> g(sT->lock);

    SnapshotRecord flush_info;
    flush_info.builder().append(input_flush_info);

    {
        std::lock_guard<std::mutex> gbb(chB->channel_blackboard_lock);
        chB->channel_blackboard.snapshot(flush_info.builder());
    }
    {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        sG->process_blackboard.snapshot(flush_info.builder());
    }
    sT->thread_blackboard.snapshot(flush_info.builder());

    Log(1).stream() << chB->name << ": Flushing Caliper data" << std::endl;

    chB->events.write_output_evt(this, chB, flush_info.view());
}

void Caliper::clear(Channel* chn)
{
    std::lock_guard<::siglock> g(sT->lock);

    chn->mP->events.clear_evt(this, chn);
}

// --- Annotation interface

void Caliper::begin(const Attribute& attr, const Variant& data)
{
    if (sT->stack_error)
        return;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    bool run_events = !(prop & CALI_ATTR_SKIP_EVENTS);

    std::lock_guard<::siglock> g(sT->lock);

    // invoke callbacks
    if (run_events)
        for (auto& channel : sG->active_channels)
            channel.mP->events.pre_begin_evt(this, channel.body(), attr, data);

    if (scope == CALI_ATTR_SCOPE_THREAD)
        handle_begin(attr, data, prop, sT->thread_blackboard, sT->tree);
    else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        handle_begin(attr, data, prop, sG->process_blackboard, sT->tree);
    }

    // invoke callbacks
    if (run_events)
        for (auto& channel : sG->active_channels)
            channel.mP->events.post_begin_evt(this, channel.body(), attr, data);
}

void Caliper::end(const Attribute& attr)
{
    if (sT->stack_error)
        return;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    bool run_events = !(prop & CALI_ATTR_SKIP_EVENTS);

    cali_id_t   key         = get_blackboard_key(attr.id(), prop);
    BlackboardEntry current = { Entry(), Entry( ) };

    std::lock_guard<::siglock> g(sT->lock);

    if (scope == CALI_ATTR_SCOPE_THREAD)
        current = load_current_entry(attr, key, sT->thread_blackboard);
    else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        current = load_current_entry(attr, key, sG->process_blackboard);
    } else
        return;

    if (current.entry.empty()) {
        sT->stack_error = true;
        return;
    }

    // invoke callbacks
    if (run_events)
        for (auto& channel : sG->active_channels)
            channel.mP->events.pre_end_evt(this, channel.body(), attr, current.entry.value());

    if (scope == CALI_ATTR_SCOPE_THREAD)
        handle_end(attr, prop, current, key, sT->thread_blackboard, sT->tree);
    else {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        handle_end(attr, prop, current, key, sG->process_blackboard, sT->tree);
    }
}

void Caliper::end_with_value_check(const Attribute& attr, const Variant& data)
{
    if (sT->stack_error)
        return;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    bool run_events = !(prop & CALI_ATTR_SKIP_EVENTS);

    cali_id_t key = get_blackboard_key(attr.id(), prop);
    BlackboardEntry current = { Entry(), Entry( ) };

    std::lock_guard<::siglock> g(sT->lock);

    if (scope == CALI_ATTR_SCOPE_THREAD)
        current = load_current_entry(attr, key, sT->thread_blackboard);
    else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        current = load_current_entry(attr, key, sG->process_blackboard);
    } else
        return;

    if (current.entry.empty() || data != current.entry.value()) {
        log_stack_value_error(current.entry, attr, data);
        sT->stack_error = true;
        return;
    }

    // invoke callbacks
    if (run_events)
        for (auto& channel : sG->active_channels)
            channel.mP->events.pre_end_evt(this, channel.body(), attr, current.entry.value());

    if (scope == CALI_ATTR_SCOPE_THREAD)
        handle_end(attr, prop, current, key, sT->thread_blackboard, sT->tree);
    else {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        handle_end(attr, prop, current, key, sG->process_blackboard, sT->tree);
    }
}

void Caliper::set(const Attribute& attr, const Variant& data)
{
    if (sT->stack_error)
        return;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    bool run_events = !(prop & CALI_ATTR_SKIP_EVENTS);

    std::lock_guard<::siglock> g(sT->lock);

    // invoke callbacks
    if (run_events)
        for (auto& channel : sG->active_channels)
            channel.mP->events.pre_set_evt(this, channel.body(), attr, data);

    if (scope == CALI_ATTR_SCOPE_THREAD)
        handle_set(attr, data, prop, sT->thread_blackboard, sT->tree);
    else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        handle_set(attr, data, prop, sG->process_blackboard, sT->tree);
    }
}

void Caliper::async_event(SnapshotView info)
{
    std::lock_guard<::siglock> g(sT->lock);

    for (auto& channel : sG->active_channels)
        channel.mP->events.async_event(this, channel.body(), info);
}

void Caliper::begin(ChannelBody* chB, const Attribute& attr, const Variant& data)
{
    int  prop       = attr.properties();
    bool run_events = !(prop & CALI_ATTR_SKIP_EVENTS);

    std::lock_guard<::siglock> g(sT->lock);

    // invoke callbacks
    if (run_events && chB->is_active)
        chB->events.pre_begin_evt(this, chB, attr, data);

    {
        std::lock_guard<std::mutex> gbb(chB->channel_blackboard_lock);
        handle_begin(attr, data, prop, chB->channel_blackboard, sT->tree);
    }

    // invoke callbacks
    if (run_events && chB->is_active)
        chB->events.post_begin_evt(this, chB, attr, data);
}

void Caliper::end(ChannelBody* chB, const Attribute& attr)
{
    int  prop       = attr.properties();
    bool run_events = !(prop & CALI_ATTR_SKIP_EVENTS);

    cali_id_t key = get_blackboard_key(attr.id(), prop);
    BlackboardEntry current = { Entry(), Entry( )};

    std::lock_guard<::siglock> g(sT->lock);

    {
        std::lock_guard<std::mutex> gbb(chB->channel_blackboard_lock);
        current = load_current_entry(attr, key, chB->channel_blackboard);
    }

    if (current.entry.empty()) {
        sT->stack_error = true;
        return;
    }

    // invoke callbacks
    if (run_events && chB->is_active)
        chB->events.pre_end_evt(this, chB, attr, current.entry.value());

    {
        std::lock_guard<std::mutex> gbb(chB->channel_blackboard_lock);
        handle_end(attr, prop, current, key, chB->channel_blackboard, sT->tree);
    }
}

void Caliper::set(ChannelBody* chB, const Attribute& attr, const Variant& data)
{
    int  prop       = attr.properties();
    bool run_events = !(prop & CALI_ATTR_SKIP_EVENTS);

    std::lock_guard<::siglock> g(sT->lock);

    // invoke callbacks
    if (run_events && chB->is_active)
        chB->events.pre_set_evt(this, chB, attr, data);

    std::lock_guard<std::mutex> gbb(chB->channel_blackboard_lock);
    handle_set(attr, data, prop, chB->channel_blackboard, sT->tree);
}

// --- Query

Entry Caliper::get(const Attribute& attr)
{
    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    cali_id_t key = get_blackboard_key(attr.id(), prop);

    std::lock_guard<::siglock> g(sT->lock);

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        return sT->thread_blackboard.get(key).get(attr);
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        return sG->process_blackboard.get(key).get(attr);
    }

    return Entry();
}

Entry Caliper::get_blackboard_entry(const Attribute& attr)
{
    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    cali_id_t key = get_blackboard_key(attr.id(), prop);

    std::lock_guard<::siglock> g(sT->lock);

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        return sT->thread_blackboard.get(key);
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        return sG->process_blackboard.get(key);
    }

    return Entry();
}

Entry Caliper::get(ChannelBody* chB, const Attribute& attr)
{
    cali_id_t key = get_blackboard_key(attr.id(), attr.properties());

    std::lock_guard<::siglock> g(sT->lock);
    std::lock_guard<std::mutex> gbb(chB->channel_blackboard_lock);

    return chB->channel_blackboard.get(key).get(attr);
}

Entry Caliper::get_path_node()
{
    Entry e;

    {
        std::lock_guard<::siglock> g(sT->lock);

        e = sT->thread_blackboard.get(REGION_KEY);

        if (e.empty()) {
            std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
            e = sG->process_blackboard.get(REGION_KEY);
        }
    }

    for (Node* node = e.node(); node; node = node->parent())
        if (get_attribute(node->attribute()).is_nested())
            return Entry(node);

    return e;
}

// --- Memory region tracking

void Caliper::memory_region_begin(
    ChannelBody*     chB,
    const void*      ptr,
    const char*      label,
    size_t           elem_size,
    size_t           ndims,
    const size_t     dims[],
    size_t           n,
    const Attribute* extra_attrs,
    const Variant*   extra_vals
)
{
    std::lock_guard<::siglock> g(sT->lock);
    chB->events.track_mem_evt(this, chB, ptr, label, elem_size, ndims, dims, n, extra_attrs, extra_vals);
}

void Caliper::memory_region_end(ChannelBody* chB, const void* ptr)
{
    std::lock_guard<::siglock> g(sT->lock);
    chB->events.untrack_mem_evt(this, chB, ptr);
}

// --- Generic entry API

void Caliper::make_record(size_t n, const Attribute attr[], const Variant value[], SnapshotBuilder& rec, Node* parent)
{
    std::lock_guard<::siglock> g(sT->lock);

    Node* node = parent;

    for (size_t i = 0; i < n; ++i)
        if (attr[i].store_as_value())
            rec.append(Entry(attr[i], value[i]));
        else
            node = sT->tree.get_child(attr[i], value[i], node);

    if (node && node != parent)
        rec.append(Entry(node));
}

Node* Caliper::make_tree_entry(size_t n, const Node* nodelist[], Node* parent)
{
    std::lock_guard<::siglock> g(sT->lock);

    return sT->tree.get_path(n, nodelist, parent);
}

Node* Caliper::make_tree_entry(const Attribute& attr, const Variant& data, Node* parent)
{
    std::lock_guard<::siglock> g(sT->lock);

    return sT->tree.get_child(attr, data, parent);
}

Node* Caliper::make_tree_entry(const Attribute& attr, size_t n, const Variant data[], Node* parent)
{
    std::lock_guard<::siglock> g(sT->lock);

    return sT->tree.get_path(attr, n, data, parent);
}

Node* Caliper::node(cali_id_t id) const
{
    // no siglock necessary
    return sT->tree.node(id);
}

Variant Caliper::exchange(const Attribute& attr, const Variant& data)
{
    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    bool is_hidden = !(prop & CALI_ATTR_HIDDEN);

    cali_id_t key = get_blackboard_key(attr.id(), prop);
    Entry entry(attr, data);

    std::lock_guard<::siglock> g(sT->lock);

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        return sT->thread_blackboard.exchange(key, entry, is_hidden).value();
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        std::lock_guard<std::mutex> gbb(sG->process_blackboard_lock);
        return sG->process_blackboard.exchange(key, entry, is_hidden).value();
    }

    return Variant();
}

//
// --- Channel API
//

Channel Caliper::create_channel(const char* name, const RuntimeConfig& cfg)
{
    std::lock_guard<::siglock> g(sT->lock);

    Log(1).stream() << "Creating channel " << name << std::endl;
    static cali_id_t next_id = 0;

    Channel channel(next_id++, name, cfg);
    sG->all_channels.emplace_back(channel);

    // Create and set key & version attributes
    begin(
        channel.body(),
        create_attribute("cali.channel", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS | CALI_ATTR_GLOBAL),
        Variant(name)
    );

    services::register_configured_services(this, &channel);

    if (channel.config().get("channel", "config_check").to_bool())
        config_sanity_check(name, channel.config());
    if (Log::verbosity() >= 3)
        channel.config().print(Log(3).stream() << "Configuration:\n");

    channel.mP->events.post_init_evt(this, &channel);

    return channel;
}

Channel Caliper::get_channel(cali_id_t id)
{
    auto it = std::find_if(sG->all_channels.begin(), sG->all_channels.end(), [id](const Channel& channel) {
        return id == channel.id();
    });

    return it == sG->all_channels.end() ? Channel() : *it;
}

std::vector<Channel> Caliper::get_all_channels()
{
    return sG->all_channels;
}

void Caliper::delete_channel(Channel& channel)
{
    std::lock_guard<::siglock> g(sT->lock);

    channel.mP->events.pre_finish_evt(this, &channel);

    Log(1).stream() << "Releasing channel " << channel.name() << std::endl;

    auto it = std::find(sG->active_channels.begin(), sG->active_channels.end(), channel);
    if (it != sG->active_channels.end())
        sG->active_channels.erase(it);
    it = std::find(sG->all_channels.begin(), sG->all_channels.end(), channel);
    if (it != sG->all_channels.end())
        sG->all_channels.erase(it);

    channel.mP->events.finish_evt(this, &channel);
}

void Caliper::activate_channel(Channel& channel)
{
    channel.mP->is_active = true;

    auto it = std::find(sG->active_channels.begin(), sG->active_channels.end(), channel);
    if (it == sG->active_channels.end())
        sG->active_channels.emplace_back(channel);

    sG->max_active_channels = std::max(sG->max_active_channels, sG->active_channels.size());
}

void Caliper::deactivate_channel(Channel& channel)
{
    auto it = std::find(sG->active_channels.begin(), sG->active_channels.end(), channel);
    if (it != sG->active_channels.end())
        sG->active_channels.erase(it);

    channel.mP->is_active = false;
}

/// \brief Release current thread
void Caliper::release_thread()
{
    std::lock_guard<::siglock> g(sT->lock);

    for (auto& channel : sG->all_channels)
        channel.mP->events.release_thread_evt(this, &channel);
}

void Caliper::finalize()
{
    std::lock_guard<::siglock> g(sT->lock);

    Log(1).stream() << "Finalizing ... " << std::endl;

    while (!sG->all_channels.empty()) {
        auto channel = sG->all_channels.front();
        if (channel.mP->flush_on_exit)
            flush_and_write(channel.body(), SnapshotView());
        delete_channel(channel);
    }
}

//
// --- Caliper constructor & singleton API
//

Caliper::Caliper() : m_is_signal(false)
{
    *this = Caliper::instance();
}

Caliper Caliper::instance()
{
    GlobalData* gPtr = nullptr;
    ThreadData* tPtr = nullptr;

    if (GlobalData::s_init_lock == 0) {
        gPtr = GlobalData::gObj.g_ptr;
        tPtr = GlobalData::tObj.t_ptr;
    } else {
        if (GlobalData::s_init_lock == 2)
            // Caliper had been initialized previously; we're past the static destructor
            return Caliper(nullptr, nullptr, true);

        std::lock_guard<std::mutex> g(GlobalData::s_init_mutex);

        Log::init();

        GlobalData::gObj.g_ptr = nullptr;
        GlobalData::tObj.t_ptr = nullptr;

        if (!GlobalData::gObj.g_ptr) {
            tPtr = new ThreadData(true /* is_initial_thread */);
            gPtr = new GlobalData(tPtr);

            GlobalData::gObj.g_ptr = gPtr;

            gPtr->add_thread_data(tPtr);
            gPtr->init();

            GlobalData::s_init_lock = 0;

            // now we can use Caliper::instance()
            ::make_default_channel();
        }
    }

    if (!tPtr) {
        tPtr = gPtr->add_thread_data(new ThreadData(false /* is_initial_thread */));
        Caliper c(gPtr, tPtr, false);

        for (auto& channel : gPtr->all_channels)
            channel.mP->events.create_thread_evt(&c, &channel);
    }

    return Caliper(gPtr, tPtr, false);
}

Caliper Caliper::sigsafe_instance()
{
    return Caliper(GlobalData::gObj.g_ptr, GlobalData::tObj.t_ptr, true);
}

Caliper::operator bool () const
{
    return (sG && sT && !(m_is_signal && sT->lock.is_locked()));
}

void Caliper::release()
{
    Caliper c;

    if (c) {
        c.finalize();
        delete GlobalData::gObj.g_ptr;
        GlobalData::tObj.t_ptr = nullptr;
    }
}

bool Caliper::is_initialized()
{
    return GlobalData::s_init_lock == 0;
}

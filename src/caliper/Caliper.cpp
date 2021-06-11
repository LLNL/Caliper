// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
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

#define SNAP_MAX 120

using namespace cali;
using namespace std;

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

// --- helper functions

void
log_invalid_cfg_value(const char* var, const char* value, const char* prefix = nullptr)
{
    Log(0).stream() << (prefix ? std::string(prefix)+": " : std::string(""))
                    << "Invalid value \"" << value << "\" for " << var
                    << std::endl;
}

int
parse_snapshot_scopes(const char* channel_name, const StringConverter& cfg)
{
    const struct {
        const char* name; cali_context_scope_t scope;
    } scopemap[] = {
        { "process", CALI_SCOPE_PROCESS },
        { "thread",  CALI_SCOPE_THREAD  },
        { "channel", CALI_SCOPE_CHANNEL }
    };

    int scopes = 0;
    auto list = cfg.to_stringlist(",.");

    for (const auto &e : scopemap) {
        auto it = std::find(list.begin(), list.end(),
                            std::string(e.name));

        if (it != list.end()) {
            scopes |= e.scope;
            list.erase(it);
        }
    }

    for (const auto &s : list)
        log_invalid_cfg_value("CALI_CHANNEL_SNAPSHOT_SCOPES", s.c_str(), channel_name);

    if (Log::verbosity() > 1) {
        std::string selected_scopes;

        for (const auto &e : scopemap)
            if (scopes & e.scope)
                selected_scopes.append(std::string(" ") + e.name);

        if (selected_scopes.empty())
            selected_scopes = " none";

        Log(2).stream() << channel_name
                        << ": snapshot scopes:" << selected_scopes
                        << std::endl;
    }

    return scopes;
}

std::ostream&
print_available_services(std::ostream& os)
{
    std::vector<std::string> services = services::get_available_services();

    int count = 0;
    for (const std::string& s : services)
        os << (count++ > 0 ? "," : "") << s;

    if (!count)
        os << "none";

    return os;
}

std::vector<Entry>
get_globals_from_blackboard(Caliper* c, const Blackboard& blackboard)
{
    SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> rec_data;
    SnapshotRecord rec(rec_data);

    blackboard.snapshot(&rec);

    std::vector<const Node*> nodes;

    SnapshotRecord::Data  data = rec.data();
    SnapshotRecord::Sizes size = rec.size();

    // Go through all process nodes and filter out the global entries
    for (size_t i = 0; i < size.n_nodes; ++i)
        for (const Node* node = data.node_entries[i]; node; node = node->parent())
            if (c->get_attribute(node->attribute()).properties() & CALI_ATTR_GLOBAL)
                nodes.push_back(node);

    // Restore original order
    std::reverse(nodes.begin(), nodes.end());

    std::vector<Entry> ret;

    if (!nodes.empty())
        ret.push_back(Entry(c->make_tree_entry(nodes.size(), nodes.data(), nullptr)));

    // Add potential AS_VALUE global entries
    for (size_t i = 0; i < size.n_immediate; ++i)
        if (c->get_attribute(data.immediate_attr[i]).properties() & CALI_ATTR_GLOBAL)
            ret.push_back(Entry(data.immediate_attr[i], data.immediate_data[i]));

    return ret;
}

void make_default_channel()
{
    //   Creates default channel (which reads env vars and/or caliper.config)
    // and initializes builtin ConfigManager during initialization

    Caliper c;

    c.create_channel("default", RuntimeConfig::get_default_config());
    internal::init_builtin_configmanager(&c);
}

} // namespace [anonymous]

//
// Caliper channel data
//

struct Channel::ChannelImpl
{
    static const ConfigSet::Entry   s_configdata[];

    std::string                     name;
    bool                            active;

    RuntimeConfig                   config;
    Events                          events;          ///< callbacks

    bool                            flush_on_exit;

    Blackboard                      channel_blackboard;

    int                             snapshot_scopes;

    ChannelImpl(const char* _name, const RuntimeConfig& cfg)
        : name(_name), active(true), config(cfg), snapshot_scopes(0)
        {
            ConfigSet cali_cfg =
                config.init("channel", s_configdata);

            flush_on_exit =
                cali_cfg.get("flush_on_exit").to_bool();
            snapshot_scopes =
                ::parse_snapshot_scopes(_name, cali_cfg.get("snapshot_scopes"));
        }

    ~ChannelImpl()
        {
            if (Log::verbosity() >= 2) {
                channel_blackboard.print_statistics( Log(2).stream() << name << " channel blackboard: " )
                    << std::endl;
            }
        }
};

const ConfigSet::Entry Channel::ChannelImpl::s_configdata[] = {
    // key, type, value, short description, long description
    { "config_check", CALI_TYPE_BOOL, "true",
      "Perform configuration sanity check at initialization",
      "Perform configuration sanity check at initialization"
    },
    { "flush_on_exit", CALI_TYPE_BOOL, "true",
      "Flush Caliper buffers at program exit",
      "Flush Caliper buffers at program exit"
    },
    { "snapshot_scopes", CALI_TYPE_STRING, "process,thread",
      "List of blackboard scopes to include in snapshots",
      "List of blackboard scopes to include in snapshots"
      " (process, thread, and channel)."
    },
    ConfigSet::Terminator
};

Channel::Channel(cali_id_t id, const char* name, const RuntimeConfig& cfg)
    : IdType(id),
      mP(new ChannelImpl(name, cfg))
{
}

Channel::~Channel()
{
}

Channel::Events&
Channel::events()
{
    return mP->events;
}

RuntimeConfig
Channel::config()
{
    return mP->config;
}

std::string
Channel::name() const
{
    return mP->name;
}

bool
Channel::is_active() const
{
    return mP->active;
}

//
// --- Caliper thread data
//

/// \brief Per-thread data for the Caliper object
struct Caliper::ThreadData
{
    MetadataTree   tree;
    ::siglock      lock;

    // This thread's blackboard
    Blackboard     thread_blackboard;

    // copy of the last process blackboard snapshot
    SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> process_snapshot_data;
    SnapshotRecord process_snapshot;
    // copy of the last channel blackboard snapshot
    SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> channel_snapshot_data;
    SnapshotRecord channel_snapshot;

    // versions of the last process blackboard and channel snapshots
    int            process_bb_count;
    int            channel_bb_count;
    // channel id of the channel snapshot copy
    int            channel_bb_id;

    bool           is_initial_thread;
    bool           stack_error;

    ThreadData(bool initial_thread = false)
        : process_snapshot(process_snapshot_data),
          process_bb_count(-1),
          channel_bb_count(-1),
          channel_bb_id(-1),
          is_initial_thread(initial_thread),
          stack_error(false)
        { }

    ~ThreadData() {
        if (Log::verbosity() >= 2)
            print_detailed_stats( Log(2).stream() );
    }

    void print_detailed_stats(std::ostream& os) {
        tree.print_statistics( os << "Releasing Caliper thread data: \n" )
            << std::endl;
        thread_blackboard.print_statistics( os << "  Thread blackboard: " )
            << std::endl;
    }

    cali_err
    log_stack_error(const Node* stack, const Attribute& attr)
    {
        stack_error = true;
        std::string stackstr;
        std::string helpstr;

        if (stack) {
            stackstr =
                "\n  but current region is\n    \"";

            const Node* attr_node = tree.node(stack->attribute());

            if (attr_node)
                stackstr.append(attr_node->data().to_string());

            stackstr.append("=");
            stackstr.append(stack->data().to_string());
            stackstr.append("\".");

            helpstr =
                "\n  Run program with CALI_SERVICES_ENABLE=validator to examine nesting errors, or"
                "\n  run with CALI_CALIPER_ALLOW_REGION_OVERLAP=true to continue at your own risk.";
        } else
            stackstr =
                "\n  but region stack is empty!";

        Log(0).stream() << "Region stack mismatch: Trying to end\n    \"" << attr.name_c_str() << "\""
                        << stackstr
                        << "\n  Ceasing region tracking!"
                        << helpstr
                        << std::endl;

        return CALI_ESTACK;
    }
};


//
// --- Caliper Global Data
//

struct Caliper::GlobalData
{
    // --- static data

    static volatile sig_atomic_t       s_init_lock;
    static std::mutex                  s_init_mutex;

    static const ConfigSet::Entry      s_configdata[];

    // --- data

    bool                               automerge;
    bool                               allow_region_overlap;

    mutable std::mutex                 attribute_lock;
    map<string, Node*>                 attribute_nodes;

    Attribute                          region_key_attr;
    Attribute                          unaligned_key_attr;

    map<string, int>                   attribute_prop_presets;
    int                                attribute_default_scope;

    Blackboard                         process_blackboard;

    vector< std::unique_ptr<Channel> > channels;

    vector< ThreadData*              > thread_data;
    std::mutex                         thread_data_lock;

    // --- constructor

    GlobalData(ThreadData* sT)
        : region_key_attr    { Attribute::invalid },
          unaligned_key_attr { Attribute::invalid },
          attribute_default_scope { CALI_ATTR_SCOPE_THREAD }
    {
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
        // prevent re-initialization
        s_init_lock = 2;

        if (Log::verbosity() >= 2) {
            process_blackboard.print_statistics( Log(2).stream() << "Process blackboard: " )
                << std::endl;
        }

        {
            std::lock_guard<std::mutex>
                g(thread_data_lock);

            std::for_each(thread_data.begin(), thread_data.end(), [](ThreadData* d){
                    delete d;
                });

            thread_data.clear();
        }

        gObj.g_ptr = nullptr;

        MetadataTree::release();

        Log(1).stream() << "Finished" << std::endl;
        Log::fini();
    }

    void parse_attribute_config(const ConfigSet& config) {
        auto preset_cfg = config.get("attribute_properties").to_stringlist();

        for (const string& s : preset_cfg) {
            auto p = s.find_first_of('=');

            if (p == string::npos)
                continue;

            int prop = cali_string2prop(s.substr(p+1).c_str());

            attribute_prop_presets.insert(make_pair(s.substr(0, p), prop));
        }

        std::string scope_str = config.get("attribute_default_scope").to_string();

        if (scope_str == "process")
            attribute_default_scope = CALI_ATTR_SCOPE_PROCESS;
        else if (scope_str == "thread")
            attribute_default_scope = CALI_ATTR_SCOPE_THREAD;
        else
            log_invalid_cfg_value("CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE", scope_str.c_str());

        automerge = config.get("automerge").to_bool();
        allow_region_overlap = config.get("allow_region_overlap").to_bool();
    }

    void init() {
        init_submodules();

        parse_attribute_config(RuntimeConfig::get_default_config().init("caliper", s_configdata));

        if (Log::verbosity() >= 2)
            print_available_services( Log(2).stream() << "Available services: " ) << std::endl;

        Caliper c(this, tObj.t_ptr, false);

        region_key_attr =
            c.create_attribute("cali.region.key", CALI_TYPE_USR, CALI_ATTR_SKIP_EVENTS);
        unaligned_key_attr =
            c.create_attribute("cali.unaligned.key", CALI_TYPE_USR, CALI_ATTR_SKIP_EVENTS);

        init_attribute_classes(&c);
        init_api_attributes(&c);

        c.set(c.create_attribute("cali.caliper.version", CALI_TYPE_STRING,
                                 CALI_ATTR_SKIP_EVENTS | CALI_ATTR_GLOBAL),
              Variant(CALIPER_VERSION));

        Log(1).stream() << "Initialized" << std::endl;
    }

    // Get the attribute key, which determines the blackboard slot for each
    // attribute. By default, we merge most attributes into two slots:
    // region_key for region-type (begin/end) attributes, unaligned_key for
    // set-type attributes. We skip stack nesting checks for unaligned
    // attributes. Immediate (as_value) and nomerge attributes get
    // their own slots.
    inline const Attribute&
    get_key(const Attribute& attr) const {
        int prop = attr.properties();

        if ((prop & CALI_ATTR_ASVALUE) || (prop & CALI_ATTR_NOMERGE) || !automerge)
            return attr;
        if (prop & CALI_ATTR_UNALIGNED)
            return unaligned_key_attr;

        return region_key_attr;
    }

    ThreadData* add_thread_data(ThreadData* t) {
        tObj.t_ptr = t;

        std::lock_guard<std::mutex>
            g(thread_data_lock);

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

        S_TLSObject()
            : t_ptr(nullptr)
            { }

        ~S_TLSObject() {
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

        ~S_GObject() {
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

volatile sig_atomic_t  Caliper::GlobalData::s_init_lock = 1;
mutex                  Caliper::GlobalData::s_init_mutex;

Caliper::GlobalData::S_GObject Caliper::GlobalData::gObj;
thread_local Caliper::GlobalData::S_TLSObject Caliper::GlobalData::tObj;

const ConfigSet::Entry Caliper::GlobalData::s_configdata[] = {
    // key, type, value, short description, long description
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
    { "attribute_default_scope", CALI_TYPE_STRING, "thread",
      "Default scope for attributes",
      "Default scope for attributes. Possible values are\n"
      "  process:   Process scope\n"
      "  thread:    Thread scope"
    },
    { "automerge", CALI_TYPE_BOOL, "true",
      "Automatically merge attributes into a common context tree",
      "Automatically merge attributes into a common context tree.\n"
      "Decreases the size of context records, but may increase\n"
      "the amount of metadata and reduce performance."
    },
    { "allow_region_overlap", CALI_TYPE_BOOL, "false",
      "Allow overlapping regions for all attributes",
      "Allow overlapping begin/end regions for all attributes."
    },

    ConfigSet::Terminator
};

//
// Caliper class definition
//

// --- Attribute interface

Attribute
Caliper::create_attribute(const std::string& name, cali_attr_type type, int prop,
                          int n_meta, const Attribute* meta_attr, const Variant* meta_val)
{
    assert(sG != 0);

    std::lock_guard<::siglock>
        g(sT->lock);

    Attribute name_attr =
        Attribute::make_attribute(sT->tree.node( 8));
    Attribute prop_attr =
        Attribute::make_attribute(sT->tree.node(10));

    assert(name_attr != Attribute::invalid);
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
        for (auto& chn : sG->channels)
            if (chn)
                chn->mP->events.create_attr_evt(this, chn.get(), attr);

    return attr;
}

bool
Caliper::attribute_exists(const std::string& name) const
{
    std::lock_guard<::siglock>
        gs(sT->lock);
    std::lock_guard<std::mutex>
        ga(sG->attribute_lock);

    return (sG->attribute_nodes.find(name) != sG->attribute_nodes.end());
}

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

Attribute
Caliper::get_attribute(cali_id_t id) const
{
    // no signal lock necessary

    return Attribute::make_attribute(sT->tree.node(id));
}

std::vector<Attribute>
Caliper::get_all_attributes() const
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

/// Dispatch memory region annotation across all active channels
void
Caliper::memory_region_begin(const void* ptr, const char* label, size_t elem_size, size_t ndim, const size_t dims[],
    size_t nextra, const Attribute* extra_attrs, const Variant* extra_vals)
{
    for (auto& chn : sG->channels)
        if (chn && chn->is_active())
            memory_region_begin(chn.get(), ptr, label, elem_size, ndim, dims, nextra, extra_attrs, extra_vals);
}

/// Dispatch memory region annotation end across all active channels
void
Caliper::memory_region_end(const void* ptr)
{
    for (auto& chn : sG->channels)
        if (chn && chn->is_active())
            memory_region_end(chn.get(), ptr);
}

///   Returns all entries with CALI_ATTR_GLOBAL set from the process
/// blackboard.
std::vector<Entry>
Caliper::get_globals()
{
    std::lock_guard<::siglock>
        g(sT->lock);

    return get_globals_from_blackboard(this, sG->process_blackboard);
}

///   Returns all entries with CALI_ATTR_GLOBAL set from the given channel's
/// and the process blackboard.
std::vector<Entry>
Caliper::get_globals(Channel* channel)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    std::vector<Entry> ret =
        get_globals_from_blackboard(this, sG->process_blackboard);
    std::vector<Entry> tmp =
        get_globals_from_blackboard(this, channel->mP->channel_blackboard);

    ret.insert(ret.end(), tmp.begin(), tmp.end());

    return ret;
}

// --- Snapshot interface

void
Caliper::pull_snapshot(Channel* channel, int scopes, const SnapshotRecord* trigger_info, SnapshotRecord* sbuf)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    // Save trigger info in snapshot buf

    if (trigger_info)
        sbuf->append(*trigger_info);

    // Invoke callbacks

    channel->mP->events.snapshot(this, channel, scopes, trigger_info, sbuf);

    // Get thread blackboard data
    if (scopes & CALI_SCOPE_THREAD) {
        sT->thread_blackboard.snapshot(sbuf);
    }

    // Get process and channel blackboard data
    if (scopes & CALI_SCOPE_PROCESS) {
        //   Check if the process or channel blackboards have been updated
        // since the last snapshot on this thread.
        //   We keep a copy of the last process/channel snapshot data in our
        // thread-local storage so we don't have to access (and lock) the
        // process/channel blackboards when they haven't changed.

        int process_bb_count_now = sG->process_blackboard.count();
        if (process_bb_count_now > sT->process_bb_count) {
            //   Process blackboard has been updated:
            // update thread-local snapshot data
            sT->process_snapshot = SnapshotRecord(sT->process_snapshot_data);
            sG->process_blackboard.snapshot(&sT->process_snapshot);

            sT->process_bb_count = process_bb_count_now;
        }

        sbuf->append(sT->process_snapshot);
    }

    if (scopes & CALI_SCOPE_CHANNEL) {
        int channel_bb_count_now = channel->mP->channel_blackboard.count();
        if (sT->channel_bb_id   != static_cast<int>(channel->id()) ||
            channel_bb_count_now > sT->channel_bb_count) {
            sT->channel_snapshot = SnapshotRecord(sT->channel_snapshot_data);
            channel->mP->channel_blackboard.snapshot(&sT->channel_snapshot);

            sT->channel_bb_count = channel_bb_count_now;
            sT->channel_bb_id    = static_cast<int>(channel->id());
        }

        sbuf->append(sT->channel_snapshot);
    }
}

void
Caliper::push_snapshot(Channel* channel, const SnapshotRecord* trigger_info)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> snapshot_data;
    SnapshotRecord sbuf(snapshot_data);

    pull_snapshot(channel, channel->mP->snapshot_scopes, trigger_info, &sbuf);

    channel->mP->events.process_snapshot(this, channel, trigger_info, &sbuf);
}

void
Caliper::flush(Channel* chn, const SnapshotRecord* flush_info, SnapshotFlushFn proc_fn)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    chn->mP->events.pre_flush_evt(this, chn, flush_info);

    if (chn->mP->events.postprocess_snapshot.empty()) {
        chn->mP->events.flush_evt(this, chn, flush_info, proc_fn);
    } else {
        chn->mP->events.flush_evt(this, chn, flush_info, [this,chn,proc_fn](CaliperMetadataAccessInterface&, const std::vector<Entry>& rec) {
                std::vector<Entry> mrec(rec);

                chn->mP->events.postprocess_snapshot(this, chn, mrec);
                proc_fn(*this, mrec);
            });
    }

    chn->mP->events.post_flush_evt(this, chn, flush_info);
}

void
Caliper::flush_and_write(Channel* channel, const SnapshotRecord* input_flush_info)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> snapshot_data;
    SnapshotRecord flush_info(snapshot_data);

    if (input_flush_info)
        flush_info.append(*input_flush_info);

    channel->mP->channel_blackboard.snapshot(&flush_info);
    sG->process_blackboard.snapshot(&flush_info);
    sT->thread_blackboard.snapshot(&flush_info);

    Log(1).stream() << channel->name() << ": Flushing Caliper data" << std::endl;

    channel->mP->events.write_output_evt(this, channel, &flush_info);
}

void
Caliper::clear(Channel* chn)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    chn->mP->events.clear_evt(this, chn);
}


// --- Annotation interface

cali_err
Caliper::begin(const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_SUCCESS;

    if (attr == Attribute::invalid)
        return CALI_EINV;
    if (sT->stack_error)
        return CALI_ESTACK;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &sT->thread_blackboard;
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &sG->process_blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        for (auto& channel : sG->channels)
            if (channel && channel->is_active())
                channel->mP->events.pre_begin_evt(this, channel.get(), attr, data);

    if (prop & CALI_ATTR_ASVALUE)
        bb->set(attr, data);
    else {
        Attribute key = sG->get_key(attr);
        bb->set(key, sT->tree.get_path(1, &attr, &data, bb->get_node(sG->get_key(attr))));
    }

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        for (auto& channel : sG->channels)
            if (channel && channel->is_active())
                channel->mP->events.post_begin_evt(this, channel.get(), attr, data);

    return ret;
}

cali_err
Caliper::end(const Attribute& attr)
{
    cali_err ret = CALI_SUCCESS;

    if (sT->stack_error)
        return CALI_ESTACK;

    Entry e = get(attr);

    if (e.is_empty())
        return sT->log_stack_error(nullptr, attr);

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &sT->thread_blackboard;
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &sG->process_blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        for (auto& channel : sG->channels)
            if (channel && channel->is_active())
                channel->mP->events.pre_end_evt(this, channel.get(), attr, e.value());

    if (prop & CALI_ATTR_ASVALUE)
        bb->unset(attr);
    else {
        Attribute key = sG->get_key(attr);
        Node*    node = bb->get_node(key);

        if (node) {
            if (node->attribute() != attr.id() && key != sG->unaligned_key_attr && !sG->allow_region_overlap)
                return sT->log_stack_error(node, attr);

            node = sT->tree.remove_first_in_path(node, attr);

            if (node == sT->tree.root())
                bb->unset(key);
            else if (node)
                bb->set(key, node);
        }

        if (!node)
            return CALI_ESTACK;
    }

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        for (auto& channel : sG->channels)
            if (channel && channel->is_active())
                channel->mP->events.post_end_evt(this, channel.get(), attr, e.value());

    return ret;
}

cali_err
Caliper::set(const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_SUCCESS;

    if (attr == Attribute::invalid)
        return CALI_EINV;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &sT->thread_blackboard;
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &sG->process_blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        for (auto& channel : sG->channels)
            if (channel && channel->is_active())
                channel->mP->events.pre_set_evt(this, channel.get(), attr, data);

    if (prop & CALI_ATTR_ASVALUE)
        bb->set(attr, data);
    else {
        Attribute key = sG->get_key(attr);
        bb->set(key, sT->tree.replace_first_in_path(bb->get_node(key), attr, data));
    }

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        for (auto& channel : sG->channels)
            if (channel && channel->is_active())
                channel->mP->events.post_set_evt(this, channel.get(), attr, data);

    return ret;
}

cali_err
Caliper::begin(Channel* channel, const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_SUCCESS;

    if (attr == Attribute::invalid)
        return CALI_EINV;

    int prop = attr.properties();
    Blackboard* bb = &channel->mP->channel_blackboard;

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS) && channel->is_active())
        channel->mP->events.pre_begin_evt(this, channel, attr, data);

    if (prop & CALI_ATTR_ASVALUE)
        bb->set(attr, data);
    else {
        Attribute key = sG->get_key(attr);
        bb->set(key, sT->tree.get_path(1, &attr, &data, bb->get_node(sG->get_key(attr))));
    }

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS) && channel->is_active())
        channel->mP->events.post_begin_evt(this, channel, attr, data);

    return ret;
}

cali_err
Caliper::end(Channel* channel, const Attribute& attr)
{
    cali_err ret = CALI_SUCCESS;

    Entry e = get(channel, attr);

    if (e.is_empty())
        return sT->log_stack_error(nullptr, attr);

    int prop = attr.properties();
    Blackboard* bb = &channel->mP->channel_blackboard;

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS) && channel->is_active())
        channel->mP->events.pre_end_evt(this, channel, attr, e.value());

    if (prop & CALI_ATTR_ASVALUE)
        bb->unset(attr);
    else {
        Attribute key = sG->get_key(attr);
        Node*    node = bb->get_node(key);

        if (node) {
            if (node->attribute() != attr.id() && key != sG->unaligned_key_attr && !sG->allow_region_overlap)
                return sT->log_stack_error(node, attr);

            node = sT->tree.remove_first_in_path(node, attr);

            if (node == sT->tree.root())
                bb->unset(key);
            else if (node)
                bb->set(key, node);
        }

        if (!node)
            return CALI_ESTACK;
    }

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS) && channel->is_active())
        channel->mP->events.post_end_evt(this, channel, attr, e.value());

    return ret;
}

cali_err
Caliper::set(Channel* channel, const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_SUCCESS;

    if (attr == Attribute::invalid)
        return CALI_EINV;

    int prop = attr.properties();
    Blackboard* bb = &channel->mP->channel_blackboard;

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS) && channel->is_active())
        channel->mP->events.pre_set_evt(this, channel, attr, data);

    if (attr.store_as_value())
        bb->set(attr, data);
    else {
        Attribute key = sG->get_key(attr);
        bb->set(key, sT->tree.replace_first_in_path(bb->get_node(key), attr, data));
    }

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS) && channel->is_active())
        channel->mP->events.post_set_evt(this, channel, attr, data);

    return ret;
}

// --- Query

Entry
Caliper::get(const Attribute& attr)
{
    if (attr == Attribute::invalid)
        return Entry::empty;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &sT->thread_blackboard;
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &sG->process_blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    if (prop & CALI_ATTR_ASVALUE)
        return Entry(attr, bb->get_immediate(attr));
    else
        return Entry(sT->tree.find_node_with_attribute(attr, bb->get_node(sG->get_key(attr))));
}

Entry
Caliper::get(Channel* channel, const Attribute& attr)
{
    if (attr == Attribute::invalid)
        return Entry::empty;

    std::lock_guard<::siglock>
        g(sT->lock);

    if (attr.store_as_value())
        return Entry(attr, channel->mP->channel_blackboard.get_immediate(attr));
    else
        return Entry(sT->tree.find_node_with_attribute(attr, channel->mP->channel_blackboard.get_node(sG->get_key(attr))));
}

// --- Memory region tracking

void
Caliper::memory_region_begin(Channel* channel, const void* ptr, const char* label, size_t elem_size, size_t ndims, const size_t dims[],
    size_t n, const Attribute* extra_attrs, const Variant* extra_vals)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    channel->mP->events.track_mem_evt(this, channel, ptr, label, elem_size, ndims, dims, n, extra_attrs, extra_vals);
}

void
Caliper::memory_region_end(Channel* channel, const void* ptr)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    channel->mP->events.untrack_mem_evt(this, channel, ptr);
}

// --- Generic entry API

void
Caliper::make_record(size_t n, const Attribute* attr, const Variant* value, SnapshotRecord& list, Node* parent)
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

Node*
Caliper::make_tree_entry(size_t n, const Node* nodelist[], Node* parent)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    return sT->tree.get_path(n, nodelist, parent);
}

Node*
Caliper::make_tree_entry(const Attribute& attr, const Variant& data, Node*  parent)
{
    if (attr.store_as_value())
        return nullptr;

    std::lock_guard<::siglock>
        g(sT->lock);

    return sT->tree.get_path(1, &attr, &data, parent);
}

Node*
Caliper::make_tree_entry(const Attribute& attr, size_t n, const Variant data[], Node* parent)
{
    if (attr.store_as_value())
        return nullptr;

    std::lock_guard<::siglock>
        g(sT->lock);

    return sT->tree.get_path(attr, n, data, parent);
}


Node*
Caliper::node(cali_id_t id) const
{
    // no siglock necessary
    return sT->tree.node(id);
}

Variant
Caliper::exchange(const Attribute& attr, const Variant& data)
{
    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;

    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &sT->thread_blackboard;
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &sG->process_blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    return bb->exchange(attr, data);
}

//
// --- Channel API
//

Channel*
Caliper::create_channel(const char* name, const RuntimeConfig& cfg)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    Log(1).stream() << "Creating channel " << name << std::endl;

    Channel* channel = new Channel(sG->channels.size(), name, cfg);
    sG->channels.emplace_back(channel);

    // Create and set key & version attributes

    begin(channel, create_attribute("cali.channel", CALI_TYPE_STRING,
                                    CALI_ATTR_SKIP_EVENTS |
                                    CALI_ATTR_GLOBAL),
          Variant(name));

    services::register_configured_services(this, channel);

    if (channel->config().get("channel", "config_check").to_bool())
        config_sanity_check(name, channel->config());
    if (Log::verbosity() >= 3)
        channel->config().print( Log(3).stream() << "Configuration:\n" );

    channel->mP->events.post_init_evt(this, channel);

    return channel;
}

Channel*
Caliper::get_channel(cali_id_t id)
{
    if (sG->channels.size() <= id)
        return nullptr;

    return sG->channels[id].get();
}

std::vector<Channel*>
Caliper::get_all_channels()
{
    std::vector<Channel*> ret;

    ret.reserve(sG->channels.size());

    for (auto &chn : sG->channels)
        if (chn)
            ret.push_back(chn.get());

    return ret;
}

void
Caliper::delete_channel(Channel* chn)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    chn->mP->events.pre_finish_evt(this, chn);

    Log(1).stream() << "Releasing channel " << chn->name() << std::endl;

    chn->mP->events.finish_evt(this, chn);
    sG->channels[chn->id()].reset();
}

void
Caliper::activate_channel(Channel* chn)
{
    chn->mP->active = true;
}

void
Caliper::deactivate_channel(Channel* chn)
{
    chn->mP->active = false;
}

/// \brief Release current thread
void
Caliper::release_thread()
{
    std::lock_guard<::siglock>
        g(sT->lock);

    for (auto &chn : sG->channels)
        if (chn)
            chn->mP->events.release_thread_evt(this, chn.get());
}

void
Caliper::finalize()
{
    std::lock_guard<::siglock>
        g(sT->lock);

    Log(1).stream() << "Finalizing ... " << std::endl;

    for (auto &chnI : sG->channels)
        if (chnI) {
            Channel* channel = chnI.get();

            if (channel->is_active() && channel->mP->flush_on_exit)
                flush_and_write(channel, nullptr);

            delete_channel(channel);
        }
}


//
// --- Caliper constructor & singleton API
//

Caliper::Caliper()
    : m_is_signal(false)
{
    *this = Caliper::instance();
}

Caliper
Caliper::instance()
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

        std::lock_guard<std::mutex>
            g(GlobalData::s_init_mutex);

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

        for (auto& chn : gPtr->channels)
            if (chn)
                chn->mP->events.create_thread_evt(&c, chn.get());
    }

    return Caliper(gPtr, tPtr, false);
}

Caliper
Caliper::sigsafe_instance()
{
    return Caliper(GlobalData::gObj.g_ptr, GlobalData::tObj.t_ptr, true);
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
        c.finalize();
        delete GlobalData::gObj.g_ptr;
        GlobalData::tObj.t_ptr = nullptr;
    }
}

bool
Caliper::is_initialized()
{
    return GlobalData::s_init_lock == 0;
}

void
Caliper::add_services(const CaliperService* s)
{
    services::add_service_specs(s);
}

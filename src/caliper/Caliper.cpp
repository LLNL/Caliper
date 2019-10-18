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

// --- helper functions

std::ostream&
print_available_services(std::ostream& os)
{
    std::vector<std::string> services = Services::get_available_services();

    int count = 0;
    for (const std::string& s : services)
        os << (count++ > 0 ? "," : "") << s;

    if (!count)
        os << "none";

    return os;
}

} // namespace

//
//   Caliper channel thread data. 
// Managed by the Caliper object.

struct Channel::ThreadData {
    // this channel's thread blackboard
    Blackboard     blackboard;

    // copy of this channel's last process blackboard snapshot
    SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> proc_snap_data;
    SnapshotRecord proc_snap;

    // version of the last process blackboard snapshot
    int            proc_bb_count;

    ThreadData()
        : proc_snap(proc_snap_data),
          proc_bb_count(0)
        { }
};

//
// Caliper channel data
//

struct Channel::ChannelImpl
{
    static const ConfigSet::Entry   s_configdata[];

    std::string                     name;
    bool                            active;
    
    // TODO: call events.create_scope_evt when creating new thread

    RuntimeConfig                   config;

    Events                          events;          ///< callbacks
    Blackboard                      blackboard;      ///< process-wide blackboard

    bool                            automerge;
    bool                            flush_on_exit;

    ChannelImpl(const char* _name, const RuntimeConfig& cfg)
        : name(_name), active(true), config(cfg)
        {
            ConfigSet cali_cfg =
                config.init("channel", s_configdata);

            automerge     = cali_cfg.get("automerge").to_bool();
            flush_on_exit = cali_cfg.get("flush_on_exit").to_bool();
        }

    ~ChannelImpl()
        {
            if (Log::verbosity() >= 2) {
                blackboard.print_statistics(
                    Log(2).stream() << "Releasing channel " << name << ":\n    ")
                    << std::endl;
            }
        }

    inline const Attribute&
    get_key(const Attribute& attr, const Attribute& key_attr) const {
        int prop = attr.properties();
        
        if (!automerge || (prop & CALI_ATTR_ASVALUE) || (prop & CALI_ATTR_NOMERGE))
            return attr;

        return key_attr;
    }
};

const ConfigSet::Entry Channel::ChannelImpl::s_configdata[] = {
    // key, type, value, short description, long description
    { "automerge", CALI_TYPE_BOOL, "true",
      "Automatically merge attributes into a common context tree",
      "Automatically merge attributes into a common context tree.\n"
      "Decreases the size of context records, but may increase\n"
      "the amount of metadata and reduce performance."
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
    MetadataTree                  tree;
    ::siglock                     lock;

    std::vector< std::unique_ptr<Channel::ThreadData> >
                                  chnT;

    bool                          is_initial_thread;

    ThreadData(size_t min_num_chn, bool initial_thread = false)
        : is_initial_thread(initial_thread)
        {
            while (min_num_chn--)
                chnT.emplace_back(new Channel::ThreadData);
        }

    ~ThreadData() {
        {
            Caliper c;
            c.release_thread();
        }
        
        if (is_initial_thread)
            Caliper::release();
            
        if (Log::verbosity() >= 2)
            print_detailed_stats( Log(2).stream() );
    }

    void print_detailed_stats(std::ostream& os) {
        tree.print_statistics( os << "Releasing Caliper thread data: \n" )
            << std::endl;

        for (size_t i = 0; i < chnT.size(); ++i) {
            if (chnT[i] && chnT[i]->blackboard.count() > 0) {
                chnT[i]->blackboard.print_statistics( os << "  channel " << i << " blackboard:\n    " )
                    << std::endl;
            }
        }
    }

    bool check_and_alloc(size_t min_num_chn, bool alloc) {
        if (alloc)
            for (size_t s = chnT.size(); s < min_num_chn; ++s)
                chnT.emplace_back(new Channel::ThreadData);

        return (chnT.size() >= min_num_chn);
    }

    inline Channel::ThreadData*
    channel_thread_data(Channel* chn) {
        return chnT[chn->id()].get();
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

    struct InitHookList {
        void          (*hook)();
        InitHookList* next;
    };

    static InitHookList*               s_init_hooks;
    
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

    mutable std::mutex                 attribute_lock;
    map<string, Node*>                 attribute_nodes;

    Attribute                          key_attr;

    map<string, int>                   attribute_prop_presets;
    int                                attribute_default_scope;

    vector< std::unique_ptr<Channel> > channels;

    // --- constructor

    GlobalData()
        : key_attr { Attribute::invalid },
          attribute_default_scope { CALI_ATTR_SCOPE_THREAD }
    {        
        channels.reserve(16);

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
        Log(1).stream() << "Finished" << std::endl;

        // prevent re-initialization
        s_init_lock = 2;
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
            Log(0).stream() << "Invalid value \"" << scope_str
                            << "\" for CALI_ATTRIBUTE_DEFAULT_SCOPE"
                            << std::endl;
    }

    void init() {
        run_init_hooks();

        parse_attribute_config(RuntimeConfig::get_default_config().init("caliper", s_configdata));
        
        Services::add_default_services();

        if (Log::verbosity() >= 2)
            print_available_services( Log(2).stream() << "Available services: " ) << std::endl;
        
        Caliper c(false);
        
        key_attr =
            c.create_attribute("cali.key.attribute", CALI_TYPE_USR, CALI_ATTR_DEFAULT);
        
        init_attribute_classes(&c);
        init_api_attributes(&c);
        
        c.create_channel("default", RuntimeConfig::get_default_config());

        Log(1).stream() << "Initialized" << std::endl;
    }
};

// --- static member initialization

volatile sig_atomic_t  Caliper::GlobalData::s_init_lock = 1;
mutex                  Caliper::GlobalData::s_init_mutex;

std::unique_ptr<Caliper::GlobalData> Caliper::sG;
thread_local std::unique_ptr<Caliper::ThreadData> Caliper::sT;

Caliper::GlobalData::InitHookList* Caliper::GlobalData::s_init_hooks = nullptr;

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
    ConfigSet::Terminator
};

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

        // Set scope to PROCESS for all global attributes
        if (prop & CALI_ATTR_GLOBAL) {
            prop &= ~CALI_ATTR_SCOPE_MASK;
            prop |= CALI_ATTR_SCOPE_PROCESS;
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

/// \brief Find an attribute by name
///
/// While it should be signal safe, we do not recommend using this function in a signal handler.
///
/// \param name The attribute name
/// \return true if the attribute exists, false otherwise

bool
Caliper::attribute_exists(const std::string& name) const
{
    std::lock_guard<::siglock>
        gs(sT->lock);
    std::lock_guard<std::mutex>
        ga(sG->attribute_lock);

    return (sG->attribute_nodes.find(name) != sG->attribute_nodes.end());
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

/// Dispatch annotation region begin across all active channels
void
Caliper::begin(const Attribute& attr, const Variant& data)
{
    for (auto& chn : sG->channels)
        if (chn && chn->is_active())
            begin(chn.get(), attr, data);
}

/// Dispatch annotation region end across all active channels
void
Caliper::end(const Attribute& attr)
{
    for (auto& chn : sG->channels)
        if (chn && chn->is_active())
            end(chn.get(), attr);
}

/// Dispatch annotation region set across all active channels
void
Caliper::set(const Attribute& attr, const Variant& data)
{
    for (auto& chn : sG->channels)
        if (chn && chn->is_active())
            set(chn.get(), attr, data);
}

/// Dispatch memory region annotation across all active channels
void
Caliper::memory_region_begin(const void* ptr, const char* label, size_t elem_size, size_t ndim, const size_t dims[])
{
    for (auto& chn : sG->channels)
        if (chn && chn->is_active())
            memory_region_begin(chn.get(), ptr, label, elem_size, ndim, dims);
}

/// Dispatch memory region annotation end across all active channels
void
Caliper::memory_region_end(const void* ptr)
{
    for (auto& chn : sG->channels)
        if (chn && chn->is_active())
            memory_region_end(chn.get(), ptr);
}

///   Returns all entries with CALI_ATTR_GLOBAL set from the given channel's
/// blackboard.

std::vector<Entry>
Caliper::get_globals(Channel* chn)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> rec_data;
    SnapshotRecord rec(rec_data);
    
    // All global attributes are process scope, so just grab the process blackboard
    chn->mP->blackboard.snapshot(&rec);

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
    return get_globals(sG->channels.front().get());
}

// --- Snapshot interface

/// \brief Trigger and return a snapshot.
///
/// This function triggers a snapshot for a given channel and returns a snapshot
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
Caliper::pull_snapshot(Channel* chn, int scopes, const SnapshotRecord* trigger_info, SnapshotRecord* sbuf)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    // Save trigger info in snapshot buf

    if (trigger_info)
        sbuf->append(*trigger_info);

    // Invoke callbacks

    chn->mP->events.snapshot(this, chn, scopes, trigger_info, sbuf);

    Channel::ThreadData* chnT = sT->channel_thread_data(chn);

    // Get thread blackboard data
    if (scopes & CALI_SCOPE_THREAD) {
        chnT->blackboard.snapshot(sbuf);
    }

    // Get process blackboard data
    if (scopes & CALI_SCOPE_PROCESS) {
        //   Check if the process blackboard has been updated since the
        // last snapshot on this thread.
        //   We keep a copy of the last process snapshot data in our
        // thread-local storage so we don't have to access (and lock) the
        // process blackboard when it hasn't changed.

        int proc_bb_count_now = chn->mP->blackboard.count();
        
        if (proc_bb_count_now > chnT->proc_bb_count) {
            //   Process blackboard has been updated:
            // update thread-local snapshot data
            chnT->proc_snap = SnapshotRecord(chnT->proc_snap_data);
            chn->mP->blackboard.snapshot(&chnT->proc_snap);

            chnT->proc_bb_count = proc_bb_count_now;
        }

        sbuf->append(chnT->proc_snap);
    }
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
Caliper::push_snapshot(Channel* chn, int scopes, const SnapshotRecord* trigger_info)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> snapshot_data;
    SnapshotRecord sbuf(snapshot_data);

    pull_snapshot(chn, scopes, trigger_info, &sbuf);

    chn->mP->events.process_snapshot(this, chn, trigger_info, &sbuf);
}


/// \brief Flush aggregation/trace buffer contents.
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
Caliper::flush_and_write(Channel* chn, const SnapshotRecord* input_flush_info)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    SnapshotRecord::FixedSnapshotRecord<SNAP_MAX> snapshot_data;
    SnapshotRecord flush_info(snapshot_data);

    if (input_flush_info)
        flush_info.append(*input_flush_info);

    chn->mP->blackboard.snapshot(&flush_info);
    sT->chnT[chn->id()]->blackboard.snapshot(&flush_info);

    Log(1).stream() << chn->name() << ": Flushing Caliper data" << std::endl;

    chn->mP->events.write_output_evt(this, chn, &flush_info);
}


/// Clear aggregation and/or trace buffers.
///
/// Clears aggregation and trace buffers. Data in those buffers
/// that has not been written yet will be lost.
///
/// \note This function is not signal safe.

void
Caliper::clear(Channel* chn)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    chn->mP->events.clear_evt(this, chn);
}


// --- Annotation interface

/// \brief Push attribute:value pair on blackboard of the given channel.
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
/// \param chn  Channel
/// \param attr Attribute key
/// \param data Value to set

cali_err
Caliper::begin(Channel* chn, const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_SUCCESS;

    if (!chn || attr == Attribute::invalid)
        return CALI_EINV;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;
    
    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &sT->chnT[chn->id()]->blackboard;
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &chn->mP->blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        chn->mP->events.pre_begin_evt(this, chn, attr, data);

    if (prop & CALI_ATTR_ASVALUE)
        bb->set(attr, data);
    else {
        Attribute key = chn->mP->get_key(attr, sG->key_attr);
        bb->set(key, sT->tree.get_path(1, &attr, &data, bb->get_node(key)));
    }
    
    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        chn->mP->events.post_begin_evt(this, chn, attr, data);

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
Caliper::end(Channel* chn, const Attribute& attr)
{
    cali_err ret = CALI_SUCCESS;

    Entry e = get(chn, attr);

    if (e.is_empty())
        return CALI_ESTACK;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;
    
    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &sT->chnT[chn->id()]->blackboard;
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &chn->mP->blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        chn->mP->events.pre_end_evt(this, chn, attr, e.value());

    if (prop & CALI_ATTR_ASVALUE)
        bb->unset(attr);
    else {
        Attribute key = chn->mP->get_key(attr, sG->key_attr);
        Node*    node = bb->get_node(key);

        if (node) {
            node = sT->tree.remove_first_in_path(node, attr);

            if (node == sT->tree.root())
                bb->unset(key);
            else if (node)
                bb->set(key, node);
        }

        if (!node)
            Log(0).stream() << "error: trying to end inactive attribute " << attr.name() << endl;
    }

    // invoke callbacks
    if (!(prop & CALI_ATTR_SKIP_EVENTS))
        chn->mP->events.post_end_evt(this, chn, attr, e.value());

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
Caliper::set(Channel* chn, const Attribute& attr, const Variant& data)
{
    cali_err ret = CALI_SUCCESS;

    if (attr == Attribute::invalid)
        return CALI_EINV;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;
    
    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &(sT->channel_thread_data(chn)->blackboard);
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &chn->mP->blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    // invoke callbacks
    if (!attr.skip_events())
        chn->mP->events.pre_set_evt(this, chn, attr, data);

    if (attr.store_as_value())
        bb->set(attr, data);
    else {
        Attribute key = chn->mP->get_key(attr, sG->key_attr);
        bb->set(key, sT->tree.replace_first_in_path(bb->get_node(key), attr, data));
    }

    // invoke callbacks
    if (!attr.skip_events())
        chn->mP->events.post_set_evt(this, chn, attr, data);

    return ret;
}

// --- Query

/// \brief Retrieve top-most entry for the given attribute key from the blackboard
///    of the given channel.
///
/// This function is signal safe.
///
/// \param attr Attribute key.
///
/// \return The top-most entry on the blackboard for the given attribute key.
///         An empty Entry object if this attribute is not set.

Entry
Caliper::get(Channel* chn, const Attribute& attr)
{
    if (attr == Attribute::invalid)
        return Entry::empty;

    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;
    
    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &sT->chnT[chn->id()]->blackboard;
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &chn->mP->blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    if (prop & CALI_ATTR_ASVALUE)
        return Entry(attr, bb->get_immediate(attr));
    else
        return Entry(sT->tree.find_node_with_attribute(attr, bb->get_node(chn->mP->get_key(attr, sG->key_attr))));
}

// --- Memory region tracking

void
Caliper::memory_region_begin(Channel* chn, const void* ptr, const char* label, size_t elem_size, size_t ndims, const size_t dims[])
{
    std::lock_guard<::siglock>
        g(sT->lock);

    chn->mP->events.track_mem_evt(this, chn, ptr, label, elem_size, ndims, dims);    
}

void
Caliper::memory_region_end(Channel* chn, const void* ptr)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    chn->mP->events.untrack_mem_evt(this, chn, ptr);
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

/// \brief Return a context tree branch for the list of values with the given attribute.
///
/// \note This function is signal safe.
///
/// \param attr   Attribute. Cannot have the AS VALUE property.
/// \param n      Number of values in \a data array
/// \param data   Array of values
/// \param parent Construct path off this parent node
///
/// \return Node pointing to the end of the new path
Node*
Caliper::make_tree_entry(const Attribute& attr, size_t n, const Variant data[], Node* parent)
{
    if (attr.store_as_value())
        return nullptr;

    std::lock_guard<::siglock>
        g(sT->lock);

    return sT->tree.get_path(attr, n, data, parent);
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
Caliper::exchange(Channel* chn, const Attribute& attr, const Variant& data)
{
    int prop  = attr.properties();
    int scope = prop & CALI_ATTR_SCOPE_MASK;
    
    Blackboard* bb = nullptr;

    if (scope == CALI_ATTR_SCOPE_THREAD) {
        bb = &sT->chnT[chn->id()]->blackboard;
    } else if (scope == CALI_ATTR_SCOPE_PROCESS) {
        bb = &chn->mP->blackboard;
    }

    std::lock_guard<::siglock>
        g(sT->lock);

    return bb->exchange(attr, data);
}

//
// --- Channel API
//

/// \brief Create a %Caliper channel with the given runtime configuration.
///
/// An channel controls %Caliper's annotation tracking and measurement 
/// activities. Multiple channels can be active at the same time, independent
/// of each other. Each channel has its own runtime configuration, 
/// blackboard, and active services.
///
/// Creating channels is \b not thread-safe. Users must make sure that no
/// %Caliper activities (e.g. annotations) are active on any program thread 
/// during channel creation.
///
/// The call returns a pointer to the created channel object. %Caliper
/// retains ownership of the channel object. This pointer becomes invalid
/// when the channel is deleted. %Caliper notifies users with the 
/// finish_evt callback when an channel is about to be deleted.
///
/// \param name Name of the channel. This is used to identify the channel
///   in %Caliper log output.
/// \param cfg The channel's runtime configuration.
/// \return Pointer to the channel. Null pointer if the channel could
///   not be created.
Channel*
Caliper::create_channel(const char* name, const RuntimeConfig& cfg)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    Channel* chn = new Channel(sG->channels.size(), name, cfg);
    sG->channels.emplace_back(chn);

    sT->check_and_alloc(sG->channels.size(), true);

    // Create and set key & version attributes

    set(chn, create_attribute("cali.caliper.version", CALI_TYPE_STRING,
                              CALI_ATTR_SKIP_EVENTS | CALI_ATTR_GLOBAL),
        Variant(CALI_TYPE_STRING, CALIPER_VERSION, sizeof(CALIPER_VERSION)));
    set(chn, create_attribute("cali.channel", CALI_TYPE_STRING,
                              CALI_ATTR_SKIP_EVENTS | CALI_ATTR_GLOBAL),
        Variant(CALI_TYPE_STRING, name, strlen(name)+1));
        
    Services::register_services(this, chn);

    Log(1).stream() << "Creating channel " << name << std::endl;

    if (chn->config().get("channel", "config_check").to_bool())
        config_sanity_check(chn->config());
    if (Log::verbosity() >= 3)
        chn->config().print( Log(3).stream() << "Configuration:\n" );

    chn->mP->events.post_init_evt(this, chn);

    return chn;
}

/// \brief Return pointer to channel object with the given ID.
///
/// The call returns a pointer to the created channel object. %Caliper
/// retains ownership of the channel object. This pointer becomes invalid
/// when the channel is deleted. %Caliper notifies users with the 
/// finish_evt callback when an channel is about to be deleted.
Channel*
Caliper::get_channel(cali_id_t id)
{
    if (sG->channels.size() <= id)
        return nullptr;

    return sG->channels[id].get();
}

/// \brief Return all existing channels (active and inactive).
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

/// \brief Delete the given channel.
///
/// Deleting channels is \b not thread-safe. Users must make sure that no
/// %Caliper activities (e.g. annotations) are active on any program thread 
/// during channel creation.
void
Caliper::delete_channel(Channel* chn)
{
    std::lock_guard<::siglock>
        g(sT->lock);

    Log(1).stream() << "Deleting channel " << chn->name() << std::endl;
    
    chn->mP->events.finish_evt(this, chn);
    sG->channels[chn->id()].reset();
}

/// \brief Activate the given channel.
///
/// Inactive channels will not track or process annotations and other  
/// blackboard updates. 
void
Caliper::activate_channel(Channel* chn)
{
    chn->mP->active = true;
}

/// \brief Deactivate the given channel.
/// \copydetails Caliper::activate_channel
void
Caliper::deactivate_channel(Channel* chn)
{
    chn->mP->active = false;
}

/// \brief Release current thread
void
Caliper::release_thread()
{
    for (auto &chn : sG->channels)
        if (chn)
            chn->mP->events.release_thread_evt(this, chn.get());
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
            sT.reset(new ThreadData(16, true /* is_initial_thread */));
            sG.reset(new GlobalData);

            sG->init();

            // NOTE: We handle Caliper exit in ThreadData destructor now
            // if (atexit(&Caliper::release) != 0)
            //     Log(0).stream() << "Unable to register exit handler";

            GlobalData::s_init_lock = 0;
        }
    }

    if (!sT) {
        sT.reset(new ThreadData(sG->channels.size()));

        Caliper c(false);

        for (auto& chn : sG->channels)
            if (chn)
                chn->mP->events.create_thread_evt(&c, chn.get());
    }

    sT->check_and_alloc(sG->channels.size(), true /* can alloc */);
    
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
    return (sG && sT && sT->check_and_alloc(sG->channels.size(), !m_is_signal) && !(m_is_signal && sT->lock.is_locked()));
}

void
Caliper::release()
{
    Caliper c;

    if (c) {
        Log(1).stream() << "Finishing ..." << std::endl;
            
        for (auto &chn : sG->channels) 
            if (chn) {
                if (chn->is_active() && chn->mP->flush_on_exit) 
                    c.flush_and_write(chn.get(), nullptr);

                c.clear(chn.get());
                chn->mP->events.finish_evt(&c, chn.get());
            }
        
        sG.reset();
    }
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

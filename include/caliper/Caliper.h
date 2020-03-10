// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Caliper.h
/// Initialization function and global data declaration

#pragma once

#include "cali_definitions.h"

#include "common/Attribute.h"
#include "common/CaliperMetadataAccessInterface.h"
#include "common/Entry.h"
#include "common/IdType.h"
#include "common/Variant.h"
#include "common/util/callback.hpp"

#include <memory>
#include <utility>

namespace cali
{

// --- Forward declarations

class  Caliper;
struct CaliperService;
class  Node;
class  RuntimeConfig;
class  SnapshotRecord;

// --- Typedefs

typedef std::function<void(CaliperMetadataAccessInterface&,const std::vector<cali::Entry>&)> SnapshotFlushFn;

/// \brief Maintain a single data collection configuration with
///    callbacks and associated measurement data.
class Channel : public IdType
{
    struct ChannelImpl;

    std::shared_ptr<ChannelImpl> mP;

    Channel(cali_id_t id, const char* name, const RuntimeConfig& cfg);

public:

    ~Channel();

    // --- Events (callback functions)

    /// \brief Holds the %Caliper callbacks for a channel.
    struct Events {
        typedef util::callback<void(Caliper*,Channel*,const Attribute&)>
            attribute_cbvec;
        typedef util::callback<void(Caliper*,Channel*,const Attribute&,const Variant&)>
            update_cbvec;
        typedef util::callback<void(Caliper*,Channel*)>
            caliper_cbvec;

        typedef util::callback<void(Caliper*,Channel*,int,const SnapshotRecord*,SnapshotRecord*)>
            snapshot_cbvec;
        typedef util::callback<void(Caliper*,Channel*,const SnapshotRecord*,const SnapshotRecord*)>
            process_snapshot_cbvec;
        typedef util::callback<void(Caliper*,Channel*,std::vector<Entry>&)>
            edit_snapshot_cbvec;

        typedef util::callback<void(Caliper*,Channel*,const SnapshotRecord*,SnapshotFlushFn)>
            flush_cbvec;
        typedef util::callback<void(Caliper*,Channel*,const SnapshotRecord*)>
            write_cbvec;

        typedef util::callback<void(Caliper*,Channel*,const void*, const char*, size_t, size_t, const size_t*,
                size_t, const Attribute*, const Variant*)>
            track_mem_cbvec;
        typedef util::callback<void(Caliper*,Channel*,const void*)>
            untrack_mem_cbvec;

        /// \brief Invoked when a new attribute has been created.
        attribute_cbvec        create_attr_evt;

        /// \brief Invoked on region begin, \e before it has been put on the blackboard.
        update_cbvec           pre_begin_evt;
        /// \brief Invoked on region begin, \e after it has been put on the blackboard.
        update_cbvec           post_begin_evt;
        /// \brief Invoked when value is set, \e before it has been put on the blackboard.
        update_cbvec           pre_set_evt;
        /// \brief Invoked when value is set, \e after it has been put on the blackboard.
        update_cbvec           post_set_evt;
        /// \brief Invoked on region end, \e before it has been removed from the blackboard.
        update_cbvec           pre_end_evt;
        /// \brief Invoked on region end, \e after it has been removed from the blackboard.
        update_cbvec           post_end_evt;

        /// \brief Invoked when a new thread context is being created.
        caliper_cbvec          create_thread_evt;
        /// \brief Invoked when a thread context is being released.
        caliper_cbvec          release_thread_evt;

        /// \brief Invoked at the end of a %Caliper channel initialization.
        ///
        /// At this point, all registered services have been initialized.
        caliper_cbvec          post_init_evt;
        /// \brief Invoked at the end of %Caliper channel finalization.
        ///
        /// At this point, other services in this channel may already be
        /// destroyed. It is no longer safe to use any %Caliper API calls for
        /// this channel. It is for local cleanup only.
        caliper_cbvec          finish_evt;

        /// \brief Invoked when a snapshot is being taken.
        ///
        /// Use this callback to take performance measurements and append them
        /// to the snapshot record.
        snapshot_cbvec         snapshot;
        /// \brief Invoked when a snapshot has been completed.
        ///
        /// Used by snapshot processing services (e.g., trace and aggregate) to
        /// store the snasphot record.
        process_snapshot_cbvec process_snapshot;

        /// \brief Invoked before flush.
        write_cbvec            pre_flush_evt;
        /// \brief Flush all snapshot records.
        flush_cbvec            flush_evt;
        /// \brief Invoked after flush.
        write_cbvec            post_flush_evt;

        /// \brief Modify snapshot records during flush.
        edit_snapshot_cbvec    postprocess_snapshot;

        /// \brief Write output.
        ///
        /// This is invoked by the Caliper::flush_and_write() API call, and
        /// causes output services (e.g., report or recorder) to trigger a
        /// flush.
        write_cbvec            write_output_evt;

        /// \brief Invoked at a memory region begin.
        track_mem_cbvec        track_mem_evt;
        /// \brief Invoked at memory region end.
        untrack_mem_cbvec      untrack_mem_evt;

        /// \brief Clear local storage (trace buffers, aggregation DB)
        caliper_cbvec          clear_evt;

        /// \brief Process events for a subscription attribute in this channel.
        ///
        /// Indicates that the given subscription attribute should be tracked
        /// in this channel. Subscription attributes are marked with the
        /// \a subscription_event meta-attribute. They are used for attributes
        /// that should be tracked in some channels but not necessarily all of
        /// them, for example with wrapper services like IO or
        /// MPI, where regions should only be tracked if the service is enabled
        /// in the given channel.
        attribute_cbvec        subscribe_attribute;
    };

    /// \brief Access the callback vectors to register callbacks for this channel.
    Events&        events();

    /// \brief Return the configuration for this channel.
    RuntimeConfig  config();

    // --- Channel management

    /// \brief Return the channel's name.
    std::string    name() const;

    /// \brief Is the channel currently active?
    ///
    /// Channels can be enabled and disabled with Caliper::activate_channel()
    /// and Caliper::deactivate_channel().
    bool           is_active() const;

    friend class Caliper;
};


/// \class Caliper
/// \brief The main interface for the caliper runtime system

class Caliper : public CaliperMetadataAccessInterface
{
    struct GlobalData;
    struct ThreadData;

    GlobalData* sG;
    ThreadData* sT;

    bool m_is_signal; // are we in a signal handler?

    Caliper(GlobalData* g, ThreadData* t, bool sig)
        : sG(g), sT(t), m_is_signal(sig)
        { }

    void release_thread();

public:

    //
    // --- Global Caliper API
    //

    /// \name Annotations (across channels)
    /// \{

    cali_err  begin(const Attribute& attr, const Variant& data);
    cali_err  end(const Attribute& attr);
    cali_err  set(const Attribute& attr, const Variant& data);

    /// \}
    /// \name Memory region tracking (across channels)
    /// \{

    void      memory_region_begin(const void* ptr, const char* label, size_t elem_size, size_t ndim, const size_t dims[],
                                  size_t n = 0, const Attribute* extra_attrs = nullptr, const Variant* extra_vals = nullptr);
    void      memory_region_end(const void* ptr);

    //
    // --- Per-channel API
    //

    /// \name Snapshot API
    /// \{

    void      push_snapshot(Channel* chn, const SnapshotRecord* trigger_info);
    void      pull_snapshot(Channel* chn, int scopes, const SnapshotRecord* trigger_info, SnapshotRecord* snapshot);

    // --- Flush and I/O API

    /// \}
    /// \name Flush and I/O
    /// \{

    void      flush(Channel* chn, const SnapshotRecord* flush_info, SnapshotFlushFn proc_fn);
    void      flush_and_write(Channel* chn, const SnapshotRecord* flush_info);

    void      clear(Channel* chn);

    // --- Annotation API

    /// \}
    /// \name Annotation (single channel)
    /// \{

    cali_err  begin(Channel* channel, const Attribute& attr, const Variant& data);
    cali_err  end(Channel* channel, const Attribute& attr);
    cali_err  set(Channel* channel, const Attribute& attr, const Variant& data);

    /// \}
    /// \name Memory region tracking (single channel)
    /// \{

    void      memory_region_begin(Channel* chn, const void* ptr, const char* label, size_t elem_size, size_t ndim, const size_t dims[],
                                  size_t n = 0, const Attribute* extra_attr = nullptr, const Variant* extra_val = nullptr);
    void      memory_region_end(Channel* chn, const void* ptr);

    /// \}
    /// \name Blackboard access
    /// \{

    Variant   exchange(const Attribute& attr, const Variant& data);

    Entry     get(const Attribute& attr);
    Entry     get(Channel* channel, const Attribute& attr);

    std::vector<Entry> get_globals();
    std::vector<Entry> get_globals(Channel* channel);

    /// \}
    /// \name Explicit snapshot record manipulation
    /// \{

    void      make_record(size_t n,
                          const Attribute  attr[],
                          const Variant    data[],
                          SnapshotRecord&  list,
                          cali::Node*      parent = nullptr);

    // --- Metadata Access Interface

    /// \}
    /// \name Context tree manipulation and metadata access
    /// \{

    size_t    num_attributes() const;

    bool      attribute_exists(const std::string& name) const;

    Attribute get_attribute(cali_id_t id) const;
    Attribute get_attribute(const std::string& name) const;

    std::vector<Attribute> get_all_attributes() const;

    Attribute create_attribute(const std::string& name,
                               cali_attr_type     type,
                               int                prop = CALI_ATTR_DEFAULT,
                               int                meta = 0,
                               const Attribute*   meta_attr = nullptr,
                               const Variant*     meta_data = nullptr);

    /// \brief Return node by id
    Node*     node(cali_id_t id) const;

    /// \brief Get or create tree branch with data from given nodes in given order
    Node*     make_tree_entry(size_t n, const Node* nodelist[], Node* parent = nullptr);

    /// \brief Get or create tree entry with given attribute/value pair
    Node*     make_tree_entry(const Attribute& attr, const Variant& value, Node* parent = nullptr);

    /// \brief Get or create tree branch with given attribute and values
    Node*     make_tree_entry(const Attribute& attr, size_t n, const Variant values[], Node* parent = nullptr);

    /// \}
    /// \name Channel API
    /// \{

    Channel* create_channel(const char* name, const RuntimeConfig& cfg);

    std::vector<Channel*> get_all_channels();

    Channel* get_channel(cali_id_t id);
    // Channel* get_channel(const char* name);

    void     delete_channel(Channel* chn);

    void     activate_channel(Channel* chn);
    void     deactivate_channel(Channel* chn);

    void     finalize();

    /// \}

    // --- Caliper API access

    Caliper();

    ~Caliper()
        { }

    Caliper(const Caliper&) = default;

    Caliper& operator = (const Caliper&) = default;

    bool is_signal() const { return m_is_signal; };

    operator bool () const;

    static Caliper instance();
    static Caliper sigsafe_instance();

    static bool    is_initialized();

    static void    release();

    /// \brief Add a list of available caliper services.
    static void    add_services(const CaliperService*);

    /// \brief Add a function that is called during %Caliper initialization.
    static void    add_init_hook(void(*fn)());

    friend struct GlobalData;
};

} // namespace cali

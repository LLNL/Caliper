// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Caliper.h
/// Initialization function and global data declaration

#pragma once

#include "cali_definitions.h"
#include "callback.hpp"
#include "SnapshotRecord.h"

#include "common/CaliperMetadataAccessInterface.h"

#include <memory>
#include <utility>

namespace cali
{

// --- Forward declarations

class Caliper;
class Node;
class RuntimeConfig;

// --- Typedefs

/// \brief A callback function to process snapshot records when flushing
///   snapshot buffers.
typedef std::function<void(CaliperMetadataAccessInterface&, const std::vector<cali::Entry>&)> SnapshotFlushFn;

/// \brief %Channel implementation details.
///
///   Maintains data (callback tables etc.) associated with a %Caliper
/// measurement configuration. While ChannelBody pointers can be safely
/// passed through the %Caliper API and callback functions, any code that
/// needs to store a channel reference should make a copy of the
/// original Channel object to properly manage lifetime.
struct ChannelBody;

/// \brief Maintain a single data collection configuration with
///   callbacks and associated measurement data.
class Channel
{
    std::shared_ptr<ChannelBody> mP;

    Channel(cali_id_t id, const char* name, const RuntimeConfig& cfg);

public:

    constexpr Channel() : mP { nullptr } {}

    Channel(const Channel&) = default;
    Channel(Channel&&)      = default;

    Channel& operator= (const Channel&) = default;
    Channel& operator= (Channel&&)      = default;

    ~Channel();

    // --- Events (callback functions)

    /// \brief Holds the %Caliper callbacks for a channel.
    struct Events {
        typedef util::callback<void(Caliper*, const Attribute&)> attribute_cbvec;

        typedef util::callback<void(Caliper*, Channel*)> caliper_cbvec;

        typedef util::callback<void(Caliper*, ChannelBody*, const Attribute&, const Variant&)> update_cbvec;
        typedef util::callback<void(Caliper*, ChannelBody*, SnapshotView)>                     event_cbvec;

        typedef util::callback<void(Caliper*, SnapshotView, SnapshotBuilder&)> snapshot_cbvec;
        typedef util::callback<void(Caliper*, SnapshotView, SnapshotView)>     process_snapshot_cbvec;
        typedef util::callback<void(Caliper*, std::vector<Entry>&)>            edit_snapshot_cbvec;
        typedef util::callback<void(Caliper*, SnapshotView, SnapshotFlushFn)>  flush_cbvec;

        typedef util::callback<
            void(Caliper*, ChannelBody*, const void*, const char*, size_t, size_t, const size_t*, size_t, const Attribute*, const Variant*)>
                                                                      track_mem_cbvec;
        typedef util::callback<void(Caliper*, ChannelBody*, const void*)> untrack_mem_cbvec;

        /// \brief Invoked when a new attribute has been created.
        attribute_cbvec create_attr_evt;

        /// \brief Invoked on region begin, \e before it has been put on the blackboard.
        update_cbvec pre_begin_evt;
        /// \brief Invoked on region begin, \e after it has been put on the blackboard.
        update_cbvec post_begin_evt;
        /// \brief Invoked when value is set, \e before it has been put on the blackboard.
        update_cbvec pre_set_evt;
        /// \brief Invoked on region end, \e before it has been removed from the blackboard.
        update_cbvec pre_end_evt;
        /// \brief Invoked for asynchronous events
        event_cbvec  async_event;

        /// \brief Invoked when a new thread context is being created.
        caliper_cbvec create_thread_evt;
        /// \brief Invoked when a thread context is being released.
        caliper_cbvec release_thread_evt;

        /// \brief Invoked at the end of a %Caliper channel initialization.
        ///
        /// At this point, all registered services have been initialized.
        caliper_cbvec post_init_evt;
        /// \brief Invoked prior to %Caliper channel finalization.
        caliper_cbvec pre_finish_evt;
        /// \brief Invoked at the end of %Caliper channel finalization.
        ///
        /// At this point, other services in this channel may already be
        /// destroyed. It is no longer safe to use any %Caliper API calls for
        /// this channel. It is for local cleanup only.
        caliper_cbvec finish_evt;

        /// \brief Invoked when a snapshot is being taken.
        ///
        /// Use this callback to take performance measurements and append them
        /// to the snapshot record.
        snapshot_cbvec snapshot;
        /// \brief Invoked when a snapshot has been completed.
        ///
        /// Used by snapshot processing services (e.g., trace and aggregate) to
        /// store the snasphot record.
        process_snapshot_cbvec process_snapshot;

        /// \brief Invoked before flush.
        event_cbvec pre_flush_evt;
        /// \brief Flush all snapshot records.
        flush_cbvec flush_evt;
        /// \brief Invoked after flush.
        event_cbvec post_flush_evt;

        /// \brief Modify snapshot records during flush.
        edit_snapshot_cbvec postprocess_snapshot;

        /// \brief Write output.
        ///
        /// This is invoked by the Caliper::flush_and_write() API call, and
        /// causes output services (e.g., report or recorder) to trigger a
        /// flush.
        event_cbvec write_output_evt;

        /// \brief Invoked at a memory region begin.
        track_mem_cbvec track_mem_evt;
        /// \brief Invoked at memory region end.
        untrack_mem_cbvec untrack_mem_evt;

        /// \brief Clear local storage (trace buffers, aggregation DB)
        caliper_cbvec clear_evt;

        /// \brief Process events for a subscription attribute in this channel.
        ///
        /// Indicates that the given subscription attribute should be tracked
        /// in this channel. Subscription attributes are marked with the
        /// \a subscription_event meta-attribute. They are used for attributes
        /// that should be tracked in some channels but not necessarily all of
        /// them, for example with wrapper services like IO or
        /// MPI, where regions should only be tracked if the service is enabled
        /// in the given channel.
        attribute_cbvec subscribe_attribute;
    };

    ChannelBody* body() { return mP.get(); }

    /// \brief Access the callback vectors to register callbacks for this channel.
    Events& events();

    /// \brief Return the configuration for this channel.
    RuntimeConfig config();

    // --- Channel management

    /// \brief Return the channel's name.
    std::string name() const;

    /// \brief Is the channel currently active?
    ///
    /// Channels can be enabled and disabled with Caliper::activate_channel()
    /// and Caliper::deactivate_channel().
    bool is_active() const;

    cali_id_t id() const;

    operator bool () const { return mP.use_count() > 0; }

    friend class Caliper;
    friend bool operator== (const Channel&, const Channel&);
    friend bool operator!= (const Channel&, const Channel&);
};

inline bool operator== (const Channel& a, const Channel& b)
{
    return a.mP == b.mP;
}

inline bool operator!= (const Channel& a, const Channel& b)
{
    return a.mP != b.mP;
}

/// \class Caliper
/// \brief Main interface for the caliper runtime system
///
///   The Caliper class provides access to the %Caliper runtime API.
/// When created, a Caliper object fetches thread-local and global information
/// for the %Caliper session. As a result, Caliper objects cannot be used
/// across thread boundaries. Always create a new Caliper object when leaving
/// thread scopes -- do not store or copy the object!
///
///   The Caliper class also facilitates signal safety via the \a is_signal
/// flag. Signal handlers and other non-reentrant functions should create
/// a Caliper object with the sigsafe_instance() method. Before executing any
/// potentially non-signal safe actions, callback functions for callbacks
/// deemed signal-safe (e.g. snapshot) must use the is_signal() method to
/// determine if non-signal safe actions are allowed in the current
/// %Caliper instance.

class Caliper : public CaliperMetadataAccessInterface
{
    struct GlobalData;
    struct ThreadData;

    GlobalData* sG;
    ThreadData* sT;

    bool m_is_signal; // are we in a signal handler?

    Caliper(GlobalData* g, ThreadData* t, bool sig) : sG(g), sT(t), m_is_signal(sig) {}

    void release_thread();

public:

    //
    // --- Global Caliper API
    //

    /// \name Annotations (across channels)
    /// \{

    /// \brief Push attribute:value pair on the process or thread blackboard.
    ///
    /// Adds the given attribute/value pair on the blackboard. Appends the
    /// value to any previous values of the same attribute, creating a
    /// hierarchy.
    ///
    /// Invokes the pre_begin/post_begin callbacks, unless \a attr has the
    /// CALI_ATTR_SKIP_EVENTS attribute property is set.
    ///
    /// This function is signal safe.
    ///
    /// \param attr Attribute key
    /// \param data Value to set
    void begin(const Attribute& attr, const Variant& data);

    /// \brief Pop/remove top-most entry with \a attr from
    ///   the process or thread blackboard.
    ///
    /// This function invokes the pre_end callback, unless \a attr has the
    /// CALI_ATTR_SKIP_EVENTS attribute property set.
    ///
    /// This function is signal safe.
    ///
    /// \param attr Attribute key.
    void end(const Attribute& attr);

    /// \brief Pop/remove top-most \a attr entry from blackboard
    ///   and check if current value is equal to \a data
    /// \copydetails Caliper::end(const Attribute&)
    void end_with_value_check(const Attribute& attr, const Variant& data);

    /// \brief Set attribute:value pair on the process or thread blackboard
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
    void set(const Attribute& attr, const Variant& data);

    /// \brief Mark an asynchronous event with the given info.
    ///
    /// This function invokes the async_event callback on all active channels.
    ///
    /// \param info Custom data to be passed into the async_event callback.
    void async_event(SnapshotView info);

    /// \}
    /// \name Memory region tracking (across channels)
    /// \{

    /// \brief Label and track memory region \a ptr
    ///
    /// Access memory region tracking functionality. Tracks memory region
    /// starting at \a ptr. Requires the \em alloc service to work.
    ///
    /// \param ptr Start of the memory region
    /// \param label A label for the memory region
    /// \param elem_size Array element size
    /// \param ndim Number of array dimensions
    /// \param dims Size of each array dimension
    /// \param n_extra Number of additional attribute:value pairs to
    ///   record for this memory region
    /// \param extra_attrs Attribute keys for additional attribute:value pairs
    /// \param estra_vals Values for additional attribute:value pairs
    void memory_region_begin(
        const void*      ptr,
        const char*      label,
        size_t           elem_size,
        size_t           ndim,
        const size_t     dims[],
        size_t           n_extra     = 0,
        const Attribute* extra_attrs = nullptr,
        const Variant*   extra_vals  = nullptr
    );

    /// \brief De-register a tracked memory region starting at \a ptr
    /// \sa memory_region_begin()
    void memory_region_end(const void* ptr);

    //
    // --- Per-channel API
    //

    /// \name Snapshot API
    /// \{

    /// \brief Trigger and process a snapshot.
    ///
    ///   Triggers a snapshot on channel \a chB and then passes the snapshot
    /// record to the process_snapshot callback.
    ///
    /// This function is signal safe.
    ///
    /// \param chB The %Caliper channel to fetch and process the snapshot.
    /// \param trigger_info A caller-provided list of attributes that is passed
    ///   to the snapshot and process_snapshot callbacks, and added to the
    ///   returned snapshot record.
    void push_snapshot(ChannelBody* chB, SnapshotView trigger_info);

    /// \brief Specialized snapshot trigger function which replaces a given blackboard entry.
    ///
    ///   Internal-use snapshot trigger function which removes a given blackboard
    /// snapshot entry before processing the record. This allows for an optimization
    /// when creating event trigger info entries where we replace the context entry
    /// with an augmented entry from \a trigger_info.
    void push_snapshot_replace(ChannelBody* chB, SnapshotView trigger_info, const Entry& target);

    /// \brief Return context data from blackboards.
    ///
    /// This function updates the caller-provided snapshot record builder
    /// with the %Caliper blackboard contents.
    ///
    /// This function is signal safe.
    ///
    /// \param rec The snapshot record buffer to update.
    void pull_context(SnapshotBuilder& rec);

    /// \brief Trigger and return a snapshot.
    ///
    /// This function triggers a snapshot for a given channel and updates the
    /// snapshot record provided the caller. The updated record contains
    /// the  current blackboard contents, measurement values provided by
    /// service modules, and the contents of the trigger_info list provided by
    /// the caller.
    ///
    /// The function invokes the snapshot callback, which instructs attached
    /// services to take measurements (e.g., a timestamp) and add them to the
    /// returned record. The caller-provided trigger_info list is passed to
    /// the snapshot callback. The returned snapshot record also contains
    /// contents of the current thread's and/or process-wide blackboard,
    /// as specified by the snapshot scopes configuration.
    ///
    /// This function is signal safe.
    ///
    /// \param chB The %Caliper channel to fetch the snapshot from.
    /// \param trigger_info A caller-provided record that is passed to the
    ///   snapshot callback, and added to the returned snapshot record.
    /// \param rec The snapshot record buffer to update.
    void pull_snapshot(ChannelBody* chB, SnapshotView trigger_info, SnapshotBuilder& rec);

    // --- Flush and I/O API

    /// \}
    /// \name Flush and I/O
    /// \{

    /// \brief Flush aggregation/trace buffer contents into the \a proc_fn
    ///   processing function.
    void flush(ChannelBody* chB, SnapshotView flush_info, SnapshotFlushFn proc_fn);

    /// \brief Flush snapshot buffer contents on \a channel into the registered
    ///   output services.
    ///
    /// This function invokes the pre_flush, flush, and flush_finish callbacks.
    ///
    /// This function is not signal safe.
    ///
    /// \param channel The channel to flush
    /// \param input_flush_info User-provided flush context information
    void flush_and_write(ChannelBody* chB, SnapshotView flush_info);

    /// \brief Clear snapshot buffers on \a channel
    ///
    /// Clears aggregation and trace buffers. Data in those buffers
    /// that has not been written yet will be lost.
    ///
    /// This function is not signal safe.
    void clear(Channel* channel);

    // --- Annotation API

    /// \}
    /// \name Annotation (single channel)
    /// \{

    /// \brief Push attribute:value pair on the channel blackboard.
    ///
    /// Adds the given attribute/value pair on \a chB's
    /// blackboard. Appends the value to any previous values of the
    /// same attribute, creating a hierarchy.
    ///
    /// This function invokes pre_begin/post_begin callbacks, unless the
    /// CALI_ATTR_SKIP_EVENTS attribute property is set in \a attr.
    ///
    /// This function is signal safe.
    ///
    /// \param chB  Designated channel
    /// \param attr Attribute key
    /// \param data Value to set
    ///
    /// \sa begin(const Attribute&, const Variant&)
    void begin(ChannelBody* chB, const Attribute& attr, const Variant& data);

    /// \brief Pop/remove top-most entry with given attribute from
    ///   the channel blackboard.
    ///
    /// This function invokes the pre_end callbacks, unless the
    /// CALI_ATTR_SKIP_EVENTS attribute property is set in \a attr.
    ///
    /// This function is signal safe.
    ///
    /// \param attr Attribute key.
    ///
    /// \sa end(const Attribute&)
    void end(ChannelBody* chB, const Attribute& attr);

    /// \brief Set attribute:value pair on the channel blackboard.
    ///
    /// Set the given attribute/value pair on the given channel's blackboard.
    /// Overwrites the previous values of the same attribute.
    ///
    /// This function invokes pre_set/post_set callbacks, unless the
    /// CALI_ATTR_SKIP_EVENTS attribute property is set in \a attr.
    ///
    /// This function is signal safe.
    ///
    /// \param attr Attribute key
    /// \param data Value to set
    /// \sa set(const Attribute&, const Variant&)
    void set(ChannelBody* chB, const Attribute& attr, const Variant& data);

    /// \}
    /// \name Memory region tracking (single channel)
    /// \{

    void memory_region_begin(
        ChannelBody*     chB,
        const void*      ptr,
        const char*      label,
        size_t           elem_size,
        size_t           ndim,
        const size_t     dims[],
        size_t           n          = 0,
        const Attribute* extra_attr = nullptr,
        const Variant*   extra_val  = nullptr
    );
    void memory_region_end(ChannelBody* chB, const void* ptr);

    /// \}
    /// \name Blackboard access
    /// \{

    /// \brief Atomically update value for \a attr on the blackboard and
    ///   return the previous value.
    ///
    /// This function is signal safe.
    ///
    /// \param attr Attribute key. Must have AS_VALUE attribute property.
    /// \param data The new value.
    ///
    /// \return The previous value \a attr
    Variant exchange(const Attribute& attr, const Variant& data);

    /// \brief Retrieve top-most entry for the given attribute key from the
    ///   process or thread blackboard.
    ///
    /// This function is signal safe.
    ///
    /// \param attr Attribute key.
    ///
    /// \return The top-most entry on the blackboard for the given attribute key.
    ///   An empty Entry object if this attribute is not set.
    Entry get(const Attribute& attr);

    /// \brief Retrieve top-most entry for the given attribute key from the
    ///   channel blackboard of the given channel.
    ///
    /// This function is signal safe.
    ///
    /// \param attr Attribute key.
    ///
    /// \return The top-most entry on the blackboard for the given attribute key.
    ///   An empty Entry object if this attribute is not set.
    Entry get(ChannelBody* chB, const Attribute& attr);

    /// \brief Retrieve the current top-most entry in \a attr's blackboard slot
    ///
    /// A special-purpose function which retrieves the current top-most entry in
    /// \a attr's blackboard slot. This is not necessarily \a attr itself because
    /// reference attributes can share a single blackboard slot. Usually
    /// \ref Caliper::get(const Attribute) should be used instead.
    Entry get_blackboard_entry(const Attribute& attr);

    /// \brief Retrieve the current path entry from the blackboard.
    Entry get_path_node();

    /// \brief Return all global attributes for the default channel
    /// \sa get_globals(Channel*)
    std::vector<Entry> get_globals();

    /// \brief Return all global attributes for \a chB
    std::vector<Entry> get_globals(ChannelBody* chB);

    /// \}
    /// \name Explicit snapshot record manipulation
    /// \{

    /// \brief Create a snapshot record (entry list) from the given
    ///   attribute:value pairs
    ///
    /// This function is signal-safe.
    ///
    /// \param n      Number of elements in attribute/value lists
    /// \param attr   Attribute list
    /// \param data   Value list
    /// \param list   Output record builder
    /// \param parent (Optional) parent node for any treee elements.
    void make_record(
        size_t           n,
        const Attribute  attr[],
        const Variant    data[],
        SnapshotBuilder& rec,
        cali::Node*      parent = nullptr
    );

    // --- Metadata Access Interface

    /// \}
    /// \name Context tree manipulation and metadata access
    /// \{

    /// \brief Return the attribute object for a given attribute ID
    Attribute get_attribute(cali_id_t id) const;
    /// \brief Return the attribute object for the given attribute name
    Attribute get_attribute(const std::string& name) const;

    /// \brief Return a vector with all attribute keys
    std::vector<Attribute> get_all_attributes() const;

    /// \brief Create a %Caliper attribute
    ///
    /// This function creates and returns an attribute key with the given
    /// name, type, and properties. Optionally, metadata can be added via
    /// additional attribute:value pairs.
    ///
    /// Attribute names must be unique. If an attribute with the given name
    /// already exists, the existing attribute is returned.
    ///
    /// After a new attribute has been created, this function invokes the
    /// create_attr_evt callback. If the attribute already exists, the
    /// callbacks will not be invoked. If two threads create an attribute
    /// with the same name simultaneously, the pre_create_attr_evt
    /// callback may be invoked on both threads, but create_attr_evt will
    /// only be invoked once. However, both threads will successfully return
    /// the new attribute.
    ///
    /// This function is not signal safe.
    ///
    /// \param name Name of the attribute
    /// \param type Type of the attribute
    /// \param prop Attribute property bitmap. Values of type
    ///   cali_attr_properties combined with bitwise or.
    /// \param n_meta Number of metadata entries
    /// \param meta_attr Metadata attribute list. An array of n_meta attribute
    ///   entries.
    /// \param meta_val Metadata values. An array of n_meta values.
    /// \return The created attribute.
    Attribute create_attribute(
        const std::string& name,
        cali_attr_type     type,
        int                prop      = CALI_ATTR_DEFAULT,
        int                meta      = 0,
        const Attribute*   meta_attr = nullptr,
        const Variant*     meta_data = nullptr
    );

    /// \brief Return node with given \a id
    Node* node(cali_id_t id) const;

    /// \brief Merge all nodes in \a nodelist into a single path under \a parent
    ///
    /// Creates a new path under \a parent with the contents of \a nodelist,
    /// if it does not yet exist.
    ///
    /// This function is signal safe.
    ///
    /// \param n        Number of nodes in node list
    /// \param nodelist List of nodes to take key:value pairs from
    /// \param parent   Construct path off this parent node
    ///
    /// \return Node pointing to the new path
    Node* make_tree_entry(size_t n, const Node* nodelist[], Node* parent = nullptr);

    /// \brief Return a context tree path for the given key:value pair
    ///
    /// This function is signal safe.
    ///
    /// \param attr   Attribute. Cannot have the AS VALUE property.
    /// \param data   Value
    /// \param parent Construct path off this parent node
    ///
    /// \return Node pointing to the end of the new path
    Node* make_tree_entry(const Attribute& attr, const Variant& value, Node* parent = nullptr);

    /// \brief Return a context tree branch for the list of values with the given attribute.
    ///
    /// This function is signal safe.
    ///
    /// \param attr   Attribute. Cannot have the AS VALUE property.
    /// \param n      Number of values in \a data array
    /// \param data   Array of values
    /// \param parent Construct path off this parent node
    ///
    /// \return Node pointing to the end of the new path
    Node* make_tree_entry(const Attribute& attr, size_t n, const Variant values[], Node* parent = nullptr);

    /// \}
    /// \name Channel API
    /// \{

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
    /// \return The channel
    Channel create_channel(const char* name, const RuntimeConfig& cfg);

    /// \brief Return all existing channels
    std::vector<Channel> get_all_channels();

    /// \brief Return the channel with the given ID or an empty Channel object.
    Channel get_channel(cali_id_t id);

    /// \brief Delete the given channel.
    ///
    /// Deleting channels is \b not thread-safe.
    void delete_channel(Channel& chn);

    /// \brief Activate the given channel.
    ///
    /// Inactive channels will not track or process annotations and other
    /// blackboard updates.
    void activate_channel(Channel& chn);

    /// \brief Deactivate the given channel.
    /// \copydetails Caliper::activate_channel
    void deactivate_channel(Channel& chn);

    /// \brief Flush and delete all channels
    void finalize();

    /// \}

    // --- Caliper API access

    /// \brief Construct a Caliper instance object.
    /// \see instance()
    Caliper();

    ~Caliper() {}

    Caliper(const Caliper&) = default;

    Caliper& operator= (const Caliper&) = default;

    /// \brief Check if this is a signal-safe %Caliper instance.
    ///
    /// Callback functions for callbacks deemed signal-safe (e.g. snapshot)
    /// must check is_signal() before executing potentially non-signal safe
    /// actions to determine if these are allowed in the current %Caliper
    /// instance.
    bool is_signal() const { return m_is_signal; };

    /// \brief Return \a true if this is a valid %Caliper instance.
    operator bool () const;

    /// \brief Construct a Caliper instance object.
    ///
    /// The Caliper instance object provides access to the Caliper API.
    /// Internally, Caliper maintains a variety of thread-local data structures.
    /// The instance object caches access to these structures. As a result,
    /// one cannot share Caliper instance objects between threads.
    /// Use Caliper instance objects only within a function context
    /// (i.e., on the stack).
    ///
    /// For use within signal handlers, use sigsafe_instance().
    /// \see sigsafe_instance()
    ///
    /// Caliper will initialize itself in the first instance object request on a
    /// process.
    ///
    /// \return Caliper instance object
    static Caliper instance();

    /// \brief Try to construct a signal-safe Caliper instance object.
    ///
    /// A signal-safe Caliper instance object sets the \a is_signal flag to
    /// instruct the API and services that only signal-safe operations can
    /// be used.
    ///
    /// The sigsafe_instance() method protects against reentry on the same
    /// thread: it returns an invalid/empty object if the current thread
    /// is already executing %Caliper code. Always ceck if the instance is
    /// valid:
    ///
    /// \code
    ///   Caliper c = Caliper::sigsafe_instance();
    ///
    ///   // Check if the instance is valid
    ///   if (!c)
    ///     return;
    ///
    ///   c.is_signal(); // true
    /// \endcode
    ///
    /// \return %Caliper instance object. An empty/invalid object
    ///   if a signal-safe instance could not be created, e.g.
    ///   when we are already inside %Caliper code.
    static Caliper sigsafe_instance();

    /// \brief Test if Caliper has been initialized yet.
    static bool is_initialized();

    /// \brief Release %Caliper. Note that %Caliper cannot be re-initialized
    ///   after it has been released.
    static void release();

    friend struct GlobalData;
};

} // namespace cali

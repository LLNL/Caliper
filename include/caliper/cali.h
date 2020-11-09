/* Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

/**
 * \file cali.h
 * \brief C API for Caliper.
 */

#ifndef CALI_CALI_H
#define CALI_CALI_H

#include "cali_definitions.h"

#include "common/cali_types.h"
#include "common/cali_variant.h"

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * --- Type definitions ------------------------------------------------
 */

/**
 * \brief Callback function to process a snapshot entry.
 *
 * Callback function definition for processing a single snapshot entry with
 * cali_unpack_snapshot() or cali_find_all_in_snapshot().
 *
 * \param user_arg User-defined argument, passed through by the parent function.
 * \param attr_id The entry's attribute ID
 * \param val The entry's value
 * \return A zero return value tells the parent function to stop processing;
 *   otherwise it will continue.
 */
typedef int (*cali_entry_proc_fn)(void* user_arg, cali_id_t attr_id, cali_variant_t val);

/*
 * --- Miscellaneous ---------------------------------------------------
 */

/**
 * \brief Return the %Caliper version string
 */
const char* cali_caliper_version();

/*
 * --- Attributes ------------------------------------------------------
 */

/**
 * \name Attribute management
 * \{
 */

/**
 * \brief Create an attribute key.
 * \param name Name of the attribute
 * \param type Type of the attribute
 * \param properties Attribute properties
 * \return Attribute id
 */
cali_id_t
cali_create_attribute(const char*     name,
                      cali_attr_type  type,
                      int             properties);

/**
 * \brief Create an attribute key with additional metadata.
 *
 * Metadata is provided via (meta-attribute id, pointer-to-data, size) in
 * the \a meta_attr_list, \a meta_val_list, and \a meta_size_list.
 * \param name Name of the attribute
 * \param type Type of the attribute
 * \param properties Attribute properties
 * \param n Number of metadata entries
 * \param meta_attr_list Attribute IDs of the metadata entries
 * \param meta_val_list  Values of the metadata entries
 * \return Attribute id
 * \sa cali_create_attribute
 */

cali_id_t
cali_create_attribute_with_metadata(const char*     name,
                                    cali_attr_type  type,
                                    int             properties,
                                    int             n,
                                    const cali_id_t meta_attr_list[],
                                    const cali_variant_t meta_val_list[]);

/**
 * \brief Find attribute by name
 * \param name Name of attribute
 * \return Attribute ID, or CALI_INV_ID if attribute was not found
 */

cali_id_t
cali_find_attribute  (const char* name);

/**
 * \brief  Return name of attribute with given ID
 * \param  attr_id Attribute id
 * \return Attribute name, or NULL if `attr_id` is not a valid attribute ID
 */
const char*
cali_attribute_name(cali_id_t attr_id);

/**
 * \brief Return the type of the attribute with given ID
 * \param attr_id Attribute id
 * \return Attribute type, or CALI_TYPE_INV if `attr_id` is not a valid attribute ID
 */
cali_attr_type
cali_attribute_type(cali_id_t attr_id);

/**
 * \brief Return attribute property flags of the attribute with given ID
 * \param attr_id Attribute id
 * \return Attribute properties, OR'ed
 */
int
cali_attribute_properties(cali_id_t attr_id);

/**
 * \}
 */

/*
 * --- Snapshot ---------------------------------------------------------
 */

/**
 * \name Taking snapshots
 * \{
 */

/**
 * \brief Take and process a snapshot on all active channels.
 *
 * Use with caution: programs actively taking snapshots should
 * probably use a dedicated channel to do so.
 *
 * \param scope Indicates which scopes (process, thread, or task) the
 *   snapshot should span
 * \param n Number of event info entries
 * \param trigger_info_attr_list Attribute IDs of event info entries
 * \param trigger_info_val_list  Values of event info entries
 */
void
cali_push_snapshot(int scope, int n,
                   const cali_id_t trigger_info_attr_list[],
                   const cali_variant_t trigger_info_val_list[]);

/**
 * \brief Take a snapshot on the given channel and push it into its
 *    processing queue.
 *
 * The snapshot will only be taken if the channel is currently
 * active.
 *
 * \param chn_id Channel to take snapshot on
 * \param scope Indicates which scopes (process, thread, or task) the
 *   snapshot should span
 * \param n Number of event info entries
 * \param trigger_info_attr_list Attribute IDs of event info entries
 * \param trigger_info_val_list  Values of event info entries
 */
void
cali_channel_push_snapshot(cali_id_t chn_id,
                           int scope, int n,
                           const cali_id_t trigger_info_attr_list[],
                           const cali_variant_t trigger_info_val_list[]);

/**
 * \brief Take a snapshot on the given channel
 *   and write it into the user-provided buffer.
 *
 * This function can be safely called from a signal handler. However,
 * it is not guaranteed to succeed. Specifically, the function will
 * fail if the signal handler interrupts already running Caliper
 * code.
 *
 * The snapshot representation returned in \a buf is valid only on the
 * local process, while Caliper is active (which is up until Caliper's
 * `finish_evt` callback is invoked).
 * It can be parsed with cali_unpack_snapshot().
 *
 * \param chn_id Channel to take the snapshot on
 * \param scope  Indicates which scopes (process, thread, or task) the
 *   snapshot should span
 * \param len    Length of the provided snapshot buffer.
 * \param buf    User-provided snapshot storage buffer.
 * \return Actual size of the snapshot representation.
 *   If this is larger than `len`, the provided buffer was too small and
 *   not all of the snapshot was returned.
 *   If this is zero, no snapshot was taken.
 */
size_t
cali_channel_pull_snapshot(cali_id_t chn_id, int scope, size_t len, unsigned char* buf);

/**
 * \}
 * \name Processing snapshot contents
 * \{
 */

/**
 * \brief Unpack a snapshot buffer.
 *
 * Unpack a snapshot that was previously obtained on the same process
 * and examine its attribute:value entries with the given \a proc_fn
 * callback function.
 *
 * The function will invoke \a proc_fn repeatedly, once for each
 * unpacked entry. \a proc_fn should return a non-zero value if it
 * wants to continue processing, otherwise processing will stop. Note
 * that snapshot processing cannot be re-started from a partially read
 * snapshot buffer position: the buffer has to be read again from the
 * beginning.
 *
 * Hierarchical values will be given to \a proc_fn in top-down order.
 *
 * \note This function is async-signal safe if \a proc_fn is
 *   async-signal safe.
 *
 * \param buf Snapshot buffer
 * \param bytes_read Number of bytes read from the buffer
 *   (i.e., length of the snapshot)
 * \param proc_fn Callback function to process individidual entries
 * \param user_arg User-defined parameter passed through to \a proc_fn
 *
 * \sa cali_pull_snapshot, cali_entry_proc_fn
 */
void
cali_unpack_snapshot(const unsigned char* buf,
                     size_t*              bytes_read,
                     cali_entry_proc_fn   proc_fn,
                     void*                user_arg);

/**
 * Return top-most value for attribute ID \a attr_id from snapshot \a buf.
 * The snapshot must have previously been obtained on the same process with
 * cali_pull_snapshot().
 *
 * \note This function is async-signal safe
 *
 * \param buf Snapshot buffer
 * \param attr_id Attribute id
 * \param bytes_read Number of bytes read from the buffer
 *   (i.e., length of the snapshot)
 * \return The top-most stacked value for the given attribute ID, or an empty
 *   variant if none was found
 */
cali_variant_t
cali_find_first_in_snapshot(const unsigned char* buf,
                            cali_id_t            attr_id,
                            size_t*              bytes_read);

/**
 * Run all entries with attribute `attr_id` in a snapshot that was previously
 * obtained on the same process through the given `proc_fn` callback function.
 *
 * \note This function is async-signal safe if `proc_fn` is async-signal safe.
 *
 * \param buf Snapshot buffer
 * \param attr_id Attribute to read from snapshot
 * \param bytes_read Number of bytes read from the buffer
 *   (i.e., length of the snapshot)
 * \param proc_fn Callback function to process individidual entries
 * \param userdata User-defined parameter passed to `proc_fn`
 */
void
cali_find_all_in_snapshot(const unsigned char* buf,
                          cali_id_t            attr_id,
                          size_t*              bytes_read,
                          cali_entry_proc_fn   proc_fn,
                          void*                userdata);

/**
 * \}
 */

/*
 * --- Blackboard access API ---------------------------------
 */

/**
 * \name Blackboard access
 * \{
 */

/**
 * \brief Return top-most value for attribute \a attr_id from the
 *   blackboard on the default channel.
 *
 * \note This function is async-signal safe.
 *
 * \param attr_id Attribute ID to find
 * \return The top-most stacked value on the blackboard for the given
 *    attribute ID, or an empty variant if it was not found
 */
cali_variant_t
cali_get(cali_id_t attr_id);

/**
 * \brief Return top-most value for attribute \a attr_id from the
 *   blackboard on the given channel.
 *
 * \note This function is async-signal safe.
 *
 * \param chn_id  Channel to grab value from
 * \param attr_id Attribute ID to find
 * \return The top-most stacked value on the blackboard for the given
 *    attribute ID, or an empty variant if it was not found
 */
cali_variant_t
cali_channel_get(cali_id_t chn_id, cali_id_t attr_id);

/**
 * \}
 */

/*
 * --- Instrumentation API -----------------------------------
 */

/**
 * \addtogroup AnnotationAPI
 * \{
 * \name Low-level source-code annotation API
 * \{
 */

/**
 * \brief Begin nested region \a name
 *
 * Begins nested region \a name using the built-in \a annotation attribute.
 * Equivalent to the macro CALI_MARK_REGION_BEGIN.
 *
 * \see cali_end_region()
 */
void
cali_begin_region(const char* name);

/**
 * \brief End nested region \a name
 *
 * Ends nested region \a name using the built-in \a annotation attribute.
 * Prints an error if \a name does not match the currently open region.
 * Equivalent to the macro CALI_MARK_REGION_END.
 *
 * \see cali_begin_region()
 */
void
cali_end_region(const char* name);

/**
 * \brief Begin region where the value for \a attr is `true` on the blackboard.
 */
void
cali_begin(cali_id_t   attr);

/**
 * \brief Begin region \a val for attribute \a attr on the blackboard.
 *
 * The region may be nested.
 */
void
cali_begin_double(cali_id_t attr, double val);
/** \copydoc cali_begin_double() */
void
cali_begin_int(cali_id_t attr, int val);
/** \copydoc cali_begin_double() */
void
cali_begin_string(cali_id_t attr, const char* val);

/**
 * End innermost open region for \a attr on the blackboard.
 */
void
cali_end  (cali_id_t   attr);

/**
 * \brief Remove innermost value for attribute \a attr from the blackboard.
 *
 * Creates a mismatch warning if the current value does not match \a val.
 * This function is primarily used by the high-level annotation API.
 *
 * \param attr Attribute ID
 * \param val  Expected value
 */

void
cali_safe_end_string(cali_id_t attr, const char* val);

/**
 * \brief Set value for attribute \a attr to \a val on the blackboard.
 */
void
cali_set  (cali_id_t   attr,
           const void* value,
           size_t      size);
/** \copybrief cali_set() */
void
cali_set_double(cali_id_t attr, double val);
/** \copybrief cali_set() */
void
cali_set_int(cali_id_t attr, int val);
/** \copybrief cali_set() */
void
cali_set_string(cali_id_t attr, const char* val);

/**
 * \brief  Begin region where the value for the attribute named \a attr_name
 *   is set to `true` on the blackboard.
 *
 * \deprecated Use cali_begin_region()
 */
void
cali_begin_byname(const char* attr_name);

/**
 * \brief Begin region \a val for attribute named \a attr_name on the blackboard.
 *
 * The region may be nested.
 */
void
cali_begin_double_byname(const char* attr_name, double val);
/** \copydoc cali_begin_double_byname() */
void
cali_begin_int_byname(const char* attr_name, int val);
/** \copydoc cali_begin_double_byname() */
void
cali_begin_string_byname(const char* attr_name, const char* val);

/**
 * \brief Set value for attribute named \a attr_name to \a val on the blackboard.
 */
void
cali_set_double_byname(const char* attr_name, double val);
/** \copydoc cali_set_double_byname() */
void
cali_set_int_byname(const char* attr_name, int val);
/** \copydoc cali_set_double_byname() */
void
cali_set_string_byname(const char* attr_name, const char* val);

/**
 * \brief End innermost open region for attribute named \a attr_name on the
 *   blackboard.
 */
void
cali_end_byname(const char* attr_name);

/**
 * \brief Set a global attribute with name \a attr_name to \a val.
 */
void
cali_set_global_double_byname(const char* attr_name, double val);
/** \copydoc cali_set_global_double_byname() */
void
cali_set_global_int_byname(const char* attr_name, int val);
/** \copydoc cali_set_global_double_byname() */
void
cali_set_global_string_byname(const char* attr_name, const char* val);
/** \copydoc cali_set_global_double_byname() */
void
cali_set_global_uint_byname(const char* attr_name, uint64_t val);

/**
 * \}
 * \}
 */

/*
 * --- Runtime system configuration
 */

/**
 * \name Runtime configuration
 * \{
 */

/**
 * \brief Pre-set a config entry in the default config.
 *
 * The entry can still be overwritten by environment variables.
 */
void
cali_config_preset(const char* key, const char* value);

/**
 * \brief Set a config entry in the default configset.
 *
 * This entry will not be overwritten by environment variables.
 */
void
cali_config_set(const char* key, const char* value);

/**
 * \brief Enable or disable reading environment variables for the default
 *   configset.
 */
void
cali_config_allow_read_env(int allow);

struct _cali_configset_t;
typedef struct _cali_configset_t* cali_configset_t;

/**
 * \brief Create a config set with the given key-value entries.
 *
 * The config set can be used in cali_create_channel() to create a new
 * channel. The config set must be deleted with cali_delete_configset().
 *
 * \sa cali_create_channel(), cali_delete_configset()
 *
 * \param keyvallist A list of key-value pairs as array of two strings
 *    that contains the profile's configuration entries. The first string
 *    in each entry is the configuration key, the second string is its
 *    value. Keys must be all uppercase. Terminate the list with two
 *    NULL entries: <tt> { NULL, NULL } </tt>.
 * \return The config set.
 */
cali_configset_t
cali_create_configset(const char* keyvallist[][2]);

/** \brief Delete a config set created with cali_create_configset(). */
void
cali_delete_configset(cali_configset_t cfg);

/** \brief Modify a config set created with cali_create_configset().
 *
 * Sets or overwrites the value for \a key with \a value.
 */
void
cali_configset_set(cali_configset_t cfg, const char* key, const char* value);

/**
 * \}
 */

/*
 * --- Channel management
 */

/**
 * \name Channel management
 * \{
 */

/**
 * \brief Create a new %Caliper channel with the given key-value
 *   configuration profile.
 *
 * An channel controls %Caliper's annotation tracking and measurement
 * activities. Multiple channels can be active at the same time, independent
 * of each other. Each channel has its own runtime configuration,
 * blackboard, and active services.
 *
 * This function creates a new channel with the given name, flags, and
 * runtime configuration. The runtime configuration is provided as a list of
 * key-value pairs and works similar to the configuration through environment
 * variables or configuration files.
 *
 * Creating channels is \b not thread-safe. Users must make sure that no
 * %Caliper activities (e.g. annotations) are active on any program thread
 * during channel creation.
 *
 * Example:
 *
 * \code
 *   const char* trace_config[][2] =
 *     { { "CALI_SERVICES_ENABLE", "event,timestamp,trace" },
 *       { "CALI_EVENT_TRIGGER",   "annotation" },
 *       { NULL, NULL }
 *     };
 *
 *   cali_configset_t cfg =
 *     cali_create_configset(trace_config);
 *
 *   //   Create a new channel "trace" but leave it inactive initially.
 *   // (By default, channels are active immediately.)
 *   cali_id_t trace_chn_id =
 *     cali_create_channel("trace", CALI_CHANNEL_LEAVE_INACTIVE, cfg);
 *
 *   cali_delete_configset(cfg);
 *
 *   // Activate the channel now.
 *   cali_activate_channel(trace_chn_id);
 * \endcode
 *
 * \param name Name of the channel. This is used to identify the channel
 *   in %Caliper log output.
 * \param flags Flags that control channel creation as bitwise-OR of
 *   cali_channel_opt flags.
 * \param keyvallist The channel's runtime configuration.
 *
 * \return ID of the created channel.
 */
cali_id_t
cali_create_channel(const char* name, int flags, const cali_configset_t cfg);

/**
 * \brief Delete a channel. Frees associated resources, e.g. blackboards,
 *   trace buffers, etc.
 *
 * Channel deletion is \b not thread-safe. Users must make sure that no
 * %Caliper activities (e.g. annotations) are active on any program thread.
 *
 * \param chn_id ID of the channel
 */
void
cali_delete_channel(cali_id_t chn_id);

/**
 * \brief Activate the (inactive) channel with the given ID.
 *
 * Only active channels will process annotations and other events.
 */
void
cali_activate_channel(cali_id_t chn_id);

/**
 * \brief Deactivate the channel with the given ID.
 *
 * Inactive channels will not track or process annotations and many
 * other events.
 *
 * \sa cali_channel_activate
 */
void
cali_deactivate_channel(cali_id_t chn_id);

/**
 * \brief Returns a non-zero value if the channel with the given ID
 *   is active, otherwise 0.
 */
int
cali_channel_is_active(cali_id_t chn_id);

/**
 * \}
 */

/*
 * --- Runtime system management
 */

/**
 * \name Reporting
 * \{
 */

/**
 * \brief Forward the default channel's aggregation or trace buffer
 *   contents to output services.
 *
 * Flushes trace buffers and/or aggreation database in the trace and
 * aggregation services, respectively. This will forward all buffered
 * snapshot records to output services, e.g., recorder, report, or
 * mpireport.
 *
 * This call only flushes the default channel (the channel configured
 * through environment variables or the caliper.config file). To flush
 * other channels, use cali_channel_flush().
 *
 * By default, the trace/aggregation buffers will not be cleared after
 * the flush. This can be changed by adding \a
 * CALI_FLUSH_CLEAR_BUFFERS to the \a flush_opts flags.
 *
 * \param flush_opts Flush options as bitwise-OR of cali_flush_opt
 *    flags. Use 0 for default behavior.
 */
void
cali_flush(int flush_opts);

/**
 * \brief Forward aggregation or trace buffer contents to output services
 *   for the given channel.
 *
 * Flushes trace buffers and/or aggreation database in the trace and
 * aggregation services, respectively. This will forward all buffered snapshot
 * records to output services, e.g., recorder, report, or mpireport.
 *
 * By default, the trace/aggregation buffers will not be cleared after the
 * flush. This can be changed by adding \a CALI_FLUSH_CLEAR_BUFFERS to
 * the \a flush_opts flags.
 *
 * \param chn_id     ID of the channel to flush
 * \param flush_opts Flush options as bitwise-OR of cali_flush_opt flags.
 *    Use 0 for default behavior.
 */
void
cali_channel_flush(cali_id_t chn_id, int flush_opts);

/**
 * \}
 */

/*
 * --- Runtime system management
 */

/**
 * \brief Initialize Caliper.
 *
 * Typically, it is not necessary to initialize Caliper explicitly.
 * Caliper will lazily initialize itself on the first Caliper API call.
 * This function is used primarily by the Caliper annotation macros,
 * to ensure that Caliper's pre-defined annotation attributes are
 * initialized.
 * It can also be used to avoid high initialization costs in the first
 * Caliper API call.
 */
void
cali_init();

/**
 * \brief  Check if Caliper is initialized on this process.
 * \return A non-zero value if Caliper is initialized, 0 if it is not initialized.
 */
int
cali_is_initialized();

/*
 * --- Macro annotation helper functions
 */

/**
 * \brief Create a loop iteration attribute for CALI_MARK_LOOP_BEGIN.
 * \param name User-defined name of the loop
 */
cali_id_t
cali_make_loop_iteration_attribute(const char* name);

#ifdef __cplusplus
} // extern "C"
#endif

//
// --- Convenience C++ API
//

#ifdef __cplusplus

#include <iostream>
#include <map>
#include <string>

namespace cali
{

typedef std::map<std::string, std::string> config_map_t;

/**
 * \brief Create a new %Caliper channel with a given key-value
 *   configuration profile.
 *
 * A channel controls %Caliper's annotation tracking and measurement
 * activities. Multiple channels can be active at the same time, independent
 * of each other. Each channel has its own runtime configuration,
 * blackboard, and active services.
 *
 * This function creates a new channel with the given name, flags, and
 * runtime configuration. The runtime configuration is provided as a list of
 * key-value pairs and works similar to the configuration through environment
 * variables or configuration files.
 *
 * Creating channels is \b not thread-safe. Users must make sure that no
 * %Caliper activities (e.g. annotations) are active on any program thread
 * during channel creation.
 *
 * Example:
 *
 * \code
 *   // Create a new channel for profiling.
 *   cali_id_t channel_id =
 *     cali::create_channel("profile", 0, {
 *          { "CALI_SERVICES_ENABLE", "aggregate,event,report,timestamp" },
 *          { "CALI_REPORT_FILENAME", "performance-report.txt" }
 *        });
 * \endcode
 *
 * \param name Name of the channel. This is used to identify the channel
 *   in %Caliper log output.
 * \param flags Flags that control channel creation as bitwise-OR of
 *   cali_channel_opt flags.
 * \param cfg The channel's runtime configuration as a key-value map.
 *
 * \return ID of the created channel.
 */
cali_id_t
create_channel(const char* name, int flags, const config_map_t& cfg);

/**
 * \brief Run a CalQL query on channel \a channel_id and write its output into
 *   the given C++ ostream.
 *
 * This call will flush a channel's trace or aggregation buffers, run a CalQL
 * query on the output, and write the query result into the given C++
 * ostream.
 *
 * Example:
 *
 * \code
 *   // Create a channel for profiling
 *   cali_id_t profile_chn =
 *     cali::create_channel("profile", 0, {
 *         { "CALI_SERVICES_ENABLE", "aggregate,event,timestamp" },
 *         { "CALI_TIMESTAMP_SNAPSHOT_DURATION", "true" }
 *       });
 *
 *   const char* query =
 *     "SELECT annotation AS Region,sum(sum#time.duration) AS Time WHERE annotation FORMAT table";
 *
 *   CALI_MARK_BEGIN("My region");
 *   // ...
 *   CALI_MARK_END("My region");
 *
 *   // Write a profile report to std::cout
 *   cali::write_report_for_query(profile_chn, query, CALI_FLUSH_CLEAR_BUFFERS, std::cout);
 * \endcode
 *
 * \param channel_id ID of the channel to flush/query.
 * \param query      The query to run in CalQL syntax.
 *    Should include at least a FORMAT statement.
 * \param flush_opts Flush options as bitwise-OR of cali_flush_opt flags.
 *    Use 0 for default behavior.
 * \param os         A C++ ostream to write the query output to.
 */
void
write_report_for_query(cali_id_t channel_id, const char* query, int flush_opts, std::ostream& os);

} // namespace cali

#endif

/* Include high-level annotation macros.
 * In C++, also includes Annotation.h header.
 */
#include "cali_macros.h"

#endif // CALI_CALI_H

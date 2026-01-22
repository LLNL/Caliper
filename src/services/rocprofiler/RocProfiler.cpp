// Copyright (c) 2015-2024, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include "../../common/util/unitfmt.h"
#include "../../common/util/demangle.h"

#include <rocprofiler-sdk/rocprofiler.h>
#include <rocprofiler-sdk/registration.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <unordered_map>
#include <sstream>

#if ROCPROFILER_VERSION_MAJOR >= 1
#define CALI_ROCPROFILER_HAVE_COUNTERS
#endif

using namespace cali;

#define ROCPROFILER_CALL(result)                                                                             \
    {                                                                                                        \
        rocprofiler_status_t CHECKSTATUS = result;                                                           \
        if (CHECKSTATUS != ROCPROFILER_STATUS_SUCCESS) {                                                     \
            std::string status_msg = rocprofiler_get_status_string(CHECKSTATUS);                             \
            std::cerr << "[" #result "][" << __FILE__ << ":" << __LINE__ << "] "                             \
                      << " failed with error code " << CHECKSTATUS << ": " << status_msg << std::endl;       \
            std::stringstream errmsg {};                                                                     \
            errmsg << "[" #result "][" << __FILE__ << ":" << __LINE__ << "] failure (" << status_msg << ")"; \
            throw std::runtime_error(errmsg.str());                                                          \
        }                                                                                                    \
    }

namespace
{

int set_external_correlation_id(
    rocprofiler_thread_id_t /*thr_id*/,
    rocprofiler_context_id_t /*ctx_id*/,
    rocprofiler_external_correlation_id_request_kind_t /*kind*/,
    rocprofiler_tracing_operation_t /*op*/,
    uint64_t /*internal_corr_id*/,
    rocprofiler_user_data_t* external_corr_id,
    void* /*user_data*/
)
{
    Caliper c;
    external_corr_id->ptr = c.get_path_node().node();
    return 0;
}

template <typename Arg, typename... Args>
auto make_array(Arg arg, Args&&... args)
{
    constexpr auto N = 1 + sizeof...(Args);
    return std::array<Arg, N> { std::forward<Arg>(arg), std::forward<Args>(args)... };
}

class RocProfilerService
{
    Attribute m_api_attr;

    Attribute m_kernel_name_attr;

    Attribute m_host_timestamp_attr;
    Attribute m_host_duration_attr;
    Attribute m_prev_timestamp_attr;

    Attribute m_activity_start_attr;
    Attribute m_activity_end_attr;
    Attribute m_activity_name_attr;
    Attribute m_activity_bytes_attr;
    Attribute m_activity_device_id_attr;
    Attribute m_activity_queue_id_attr;
    Attribute m_activity_duration_attr;
    Attribute m_activity_count_attr;
    Attribute m_src_agent_attr;
    Attribute m_dst_agent_attr;
    Attribute m_agent_attr;
    Attribute m_bytes_attr;
    Attribute m_dispatch_id_attr;

    Attribute m_flush_region_attr;

    bool m_enable_api_callbacks       = false;
    bool m_enable_activity_tracing    = false;
    bool m_enable_snapshot_timestamps = false;
    bool m_enable_allocation_tracing  = false;
    bool m_enable_counters            = false;

    unsigned m_num_activity_records = 0;
    unsigned m_num_counter_records  = 0;
    unsigned m_failed_correlations  = 0;

    std::unordered_map<uint64_t, std::string> m_kernel_info;
    std::mutex m_kernel_info_mutex;

    std::unordered_map<uint64_t, const rocprofiler_agent_t*> m_agent_info_map;

#ifdef CALI_ROCPROFILER_HAVE_COUNTERS
    std::unordered_map<uint64_t, rocprofiler_counter_config_id_t> m_counter_profile_map;
    std::unordered_map<uint64_t, Attribute> m_counter_attr_map;

    std::unordered_map<rocprofiler_dispatch_id_t, Entry> m_counter_dispatch_correlation_map;
    std::mutex m_counter_dispatch_correlation_mutex;

    struct CounterDimensionData {
        rocprofiler_counter_record_dimension_info_t info;
        Attribute attr;
    };

    std::unordered_map<uint64_t, std::vector<CounterDimensionData>> m_counter_dimension_info_map;
#endif

    Channel m_channel;

    static RocProfilerService* s_instance;

    static rocprofiler_context_id_t s_hip_api_ctx;
    static rocprofiler_context_id_t s_activity_ctx;
    static rocprofiler_context_id_t s_rocprofiler_ctx;
    static rocprofiler_context_id_t s_alloc_tracing_ctx;
    static rocprofiler_context_id_t s_counter_ctx;

    static rocprofiler_buffer_id_t  s_activity_buf;

    void create_attributes(Caliper* c)
    {
        Attribute subs_attr = c->get_attribute("subscription_event");
        Variant   v_true(true);

        m_api_attr = c->create_attribute("rocm.api", CALI_TYPE_STRING, CALI_ATTR_NESTED, 1, &subs_attr, &v_true);

        m_activity_start_attr =
            c->create_attribute("rocm.starttime", CALI_TYPE_UINT, CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
        m_activity_end_attr =
            c->create_attribute("rocm.endtime", CALI_TYPE_UINT, CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);

        m_activity_duration_attr = c->create_attribute(
            "rocm.activity.duration",
            CALI_TYPE_UINT,
            CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE
        );
        m_activity_count_attr = c->create_attribute(
            "rocm.activity.count",
            CALI_TYPE_UINT,
            CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE
        );

        m_host_timestamp_attr = c->create_attribute(
            "rocm.host.timestamp",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS
        );
        m_prev_timestamp_attr = c->create_attribute(
            "rocm.prev.timestamp",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN
        );
        m_host_duration_attr = c->create_attribute(
            "rocm.host.duration",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE
        );
        m_bytes_attr = c->create_attribute(
            "rocm.bytes",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_AGGREGATABLE
        );

        m_activity_name_attr      = c->create_attribute("rocm.activity", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        m_activity_queue_id_attr  = c->create_attribute("rocm.activity.queue", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        m_activity_device_id_attr = c->create_attribute("rocm.activity.device", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        m_activity_bytes_attr     = c->create_attribute("rocm.activity.bytes", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        m_kernel_name_attr        = c->create_attribute("rocm.kernel.name", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        m_src_agent_attr          = c->create_attribute("rocm.src.agent", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        m_dst_agent_attr          = c->create_attribute("rocm.dst.agent", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        m_agent_attr              = c->create_attribute("rocm.agent", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);

        m_dispatch_id_attr = c->create_attribute(
            "rocm.dispatch_id",
            CALI_TYPE_UINT,
            CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS
        );

        m_flush_region_attr =
            c->create_attribute("rocprofiler.flush", CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_DEFAULT);
    }

    void update_kernel_info(uint64_t kernel_id, const std::string& name)
    {
        std::lock_guard<std::mutex> g(m_kernel_info_mutex);
        m_kernel_info.emplace(kernel_id, name);
    }

    void pre_flush_cb()
    {
        if (s_activity_buf.handle > 0)
            ROCPROFILER_CALL(rocprofiler_flush_buffer(s_activity_buf));
    }

    static void tool_tracing_callback(
        rocprofiler_context_id_t      context,
        rocprofiler_buffer_id_t       buffer_id,
        rocprofiler_record_header_t** headers,
        size_t                        num_headers,
        void*                         user_data,
        uint64_t                      drop_count
    )
    {
        if (!s_instance)
            return;

        Caliper c;
        c.begin(s_instance->m_flush_region_attr, Variant("ROCPROFILER FLUSH"));

        Entry mpi_rank_entry;

        {
            Attribute mpi_rank_attr = c.get_attribute("mpi.rank");
            if (mpi_rank_attr)
                mpi_rank_entry = c.get(mpi_rank_attr);
        }

#ifdef CALI_ROCPROFILER_HAVE_COUNTERS
        Entry counter_dispatch_entry;
#endif

        for (size_t i = 0; i < num_headers; ++i) {
            auto* header = headers[i];

            if (header->category == ROCPROFILER_BUFFER_CATEGORY_TRACING
                && header->kind == ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH) {

                auto* record = static_cast<rocprofiler_buffer_tracing_kernel_dispatch_record_t*>(header->payload);

                const Attribute attr[] = {
                    s_instance->m_activity_name_attr,
                    s_instance->m_agent_attr,
                    s_instance->m_kernel_name_attr,
                    s_instance->m_activity_start_attr,
                    s_instance->m_activity_end_attr,
                    s_instance->m_activity_duration_attr,
                    s_instance->m_activity_count_attr,
                    s_instance->m_dispatch_id_attr
                };

                const char* activity_name = nullptr;
                uint64_t    activity_name_len = 0;
                ROCPROFILER_CALL(rocprofiler_query_buffer_tracing_kind_operation_name(
                    record->kind,
                    record->operation,
                    &activity_name,
                    &activity_name_len
                ));

                Variant v_kernel_name;

                {
                    std::lock_guard<std::mutex> g(s_instance->m_kernel_info_mutex);
                    auto it = s_instance->m_kernel_info.find(record->dispatch_info.kernel_id);
                    if (it != s_instance->m_kernel_info.end())
                        v_kernel_name = Variant(CALI_TYPE_STRING, it->second.data(), it->second.size());
                    else
                        v_kernel_name = Variant(CALI_TYPE_STRING, "UNKNOWN", 7);
                }

                uint64_t agent = s_instance->m_agent_info_map.at(record->dispatch_info.agent_id.handle)->logical_node_id;

                const Variant data[] = {
                    Variant(CALI_TYPE_STRING, activity_name, activity_name_len),
                    Variant(cali_make_variant_from_uint(agent)),
                    v_kernel_name,
                    Variant(cali_make_variant_from_uint(record->start_timestamp)),
                    Variant(cali_make_variant_from_uint(record->end_timestamp)),
                    Variant(cali_make_variant_from_uint(record->end_timestamp - record->start_timestamp)),
                    Variant(cali_make_variant_from_uint(1)),
                    Variant(cali_make_variant_from_uint(record->dispatch_info.dispatch_id))
                };

                cali::Node* correlation = static_cast<cali::Node*>(record->correlation_id.external.ptr);

                FixedSizeSnapshotRecord<10> snapshot;
                c.make_record(8, attr, data, snapshot.builder(), correlation);
                if (!mpi_rank_entry.empty())
                    snapshot.builder().append(mpi_rank_entry);

                s_instance->m_channel.events().process_snapshot(&c, SnapshotView(), snapshot.view());

                ++s_instance->m_num_activity_records;
            } else if (header->category == ROCPROFILER_BUFFER_CATEGORY_TRACING && header->kind == ROCPROFILER_BUFFER_TRACING_MEMORY_COPY) {

                auto* record = static_cast<rocprofiler_buffer_tracing_memory_copy_record_t*>(header->payload);

                const Attribute attr[] = { s_instance->m_activity_name_attr, s_instance->m_activity_start_attr,
                                           s_instance->m_activity_end_attr,  s_instance->m_activity_duration_attr,
                                           s_instance->m_src_agent_attr,     s_instance->m_dst_agent_attr,
                                           s_instance->m_bytes_attr,         s_instance->m_activity_count_attr
                                         };

                const char* activity_name = nullptr;
                uint64_t    len;
                ROCPROFILER_CALL(rocprofiler_query_buffer_tracing_kind_operation_name(
                    record->kind,
                    record->operation,
                    &activity_name,
                    &len
                ));

                uint64_t src_agent = s_instance->m_agent_info_map.at(record->src_agent_id.handle)->logical_node_id;
                uint64_t dst_agent = s_instance->m_agent_info_map.at(record->dst_agent_id.handle)->logical_node_id;

                const Variant data[] = {
                    Variant(CALI_TYPE_STRING, activity_name, len),
                    Variant(cali_make_variant_from_uint(record->start_timestamp)),
                    Variant(cali_make_variant_from_uint(record->end_timestamp)),
                    Variant(cali_make_variant_from_uint(record->end_timestamp - record->start_timestamp)),
                    Variant(cali_make_variant_from_uint(src_agent)),
                    Variant(cali_make_variant_from_uint(dst_agent)),
                    Variant(cali_make_variant_from_uint(record->bytes)),
                    Variant(cali_make_variant_from_uint(1))
                };

                cali::Node* correlation = static_cast<cali::Node*>(record->correlation_id.external.ptr);

                FixedSizeSnapshotRecord<10> snapshot;
                c.make_record(8, attr, data, snapshot.builder(), correlation);
                if (!mpi_rank_entry.empty())
                    snapshot.builder().append(mpi_rank_entry);

                s_instance->m_channel.events().process_snapshot(&c, SnapshotView(), snapshot.view());

                ++s_instance->m_num_activity_records;
#ifdef CALI_ROCPROFILER_HAVE_COUNTERS
            } else if (header->category == ROCPROFILER_BUFFER_CATEGORY_COUNTERS &&
                header->kind == ROCPROFILER_COUNTER_RECORD_PROFILE_COUNTING_DISPATCH_HEADER) {

                auto* record = static_cast<rocprofiler_dispatch_counting_service_record_t*>(header->payload);

                //   external correlation ptr lookup for counter dispatch record does not work as of ROCm 7.0
                // cali::Node* correlation = static_cast<cali::Node*>(record->correlation_id.external.ptr);

                cali::Node* correlation = nullptr;

                {
                    std::lock_guard<std::mutex> g(s_instance->m_counter_dispatch_correlation_mutex);
                    auto it = s_instance->m_counter_dispatch_correlation_map.find(record->dispatch_info.dispatch_id);
                    if (it != s_instance->m_counter_dispatch_correlation_map.end()) {
                        correlation = it->second.node();
                        s_instance->m_counter_dispatch_correlation_map.erase(it);
                    } else
                        ++s_instance->m_failed_correlations;
                }

                Variant v_kernel_name;

                {
                    std::lock_guard<std::mutex> g(s_instance->m_kernel_info_mutex);
                    auto it = s_instance->m_kernel_info.find(record->dispatch_info.kernel_id);
                    if (it != s_instance->m_kernel_info.end())
                        v_kernel_name = Variant(CALI_TYPE_STRING, it->second.data(), it->second.size());
                    else
                        v_kernel_name = Variant(CALI_TYPE_STRING, "UNKNOWN", 7);
                }

                FixedSizeSnapshotRecord<4> snapshot;
                c.make_record(1, &s_instance->m_kernel_name_attr, &v_kernel_name, snapshot.builder(), correlation);

                counter_dispatch_entry = snapshot.view()[0];
            } else if (header->category == ROCPROFILER_BUFFER_CATEGORY_COUNTERS &&
                header->kind == ROCPROFILER_COUNTER_RECORD_VALUE) {
                auto* record = static_cast<rocprofiler_counter_record_t*>(header->payload);

                rocprofiler_counter_id_t counter_id = { .handle = 0 };
                rocprofiler_query_record_counter_id(record->id, &counter_id);

                FixedSizeSnapshotRecord<4> snapshot;

                if (!counter_dispatch_entry.empty()) {
                    cali::Node* correlation_entry_node = counter_dispatch_entry.node();

                    auto it = s_instance->m_counter_dimension_info_map.find(counter_id.handle);
                    if (it != s_instance->m_counter_dimension_info_map.end()) {
                        for (const auto& dim : it->second) {
                            size_t pos = 0;
                            rocprofiler_query_record_dimension_position(record->id, dim.info.id, &pos);
                            correlation_entry_node = c.make_tree_entry(dim.attr,
                                Variant(cali_make_variant_from_uint(pos)), correlation_entry_node);
                        }
                    }

                    snapshot.builder().append(Entry(correlation_entry_node));
                }

                if (!mpi_rank_entry.empty())
                    snapshot.builder().append(mpi_rank_entry);

                snapshot.builder().append(s_instance->m_dispatch_id_attr, cali_make_variant_from_uint(record->dispatch_id));

                {
                    auto it = s_instance->m_counter_attr_map.find(counter_id.handle);
                    if (it != s_instance->m_counter_attr_map.end())
                        snapshot.builder().append(it->second, cali_make_variant_from_double(record->counter_value));
                }

                s_instance->m_channel.events().process_snapshot(&c, SnapshotView(), snapshot.view());

                ++s_instance->m_num_counter_records;
#endif // CALI_ROCPROFILER_HAVE_COUNTERS
            }
        }

        c.end(s_instance->m_flush_region_attr);
    }

    static void tool_api_callback(
        rocprofiler_callback_tracing_record_t record,
        rocprofiler_user_data_t*              user_data,
        void* /* callback_data */
    )
    {
        if (!s_instance)
            return;

        if (record.kind == ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT) {
            if (record.operation == ROCPROFILER_CODE_OBJECT_DEVICE_KERNEL_SYMBOL_REGISTER
                && record.phase == ROCPROFILER_CALLBACK_PHASE_LOAD) {
                auto* data =
                    static_cast<rocprofiler_callback_tracing_code_object_kernel_symbol_register_data_t*>(record.payload
                    );
                s_instance->update_kernel_info(data->kernel_id, util::demangle(data->kernel_name));
            }
        } else {
            if (record.phase == ROCPROFILER_CALLBACK_PHASE_ENTER) {
                const char* name = nullptr;
                uint64_t    len  = 0;
                ROCPROFILER_CALL(
                    rocprofiler_query_callback_tracing_kind_operation_name(record.kind, record.operation, &name, &len)
                );
                if (!name) {
                    name = "UNKNOWN";
                    len  = 7;
                }
                Caliper::instance().begin(s_instance->m_api_attr, Variant(CALI_TYPE_STRING, name, len));
            } else if (record.phase == ROCPROFILER_CALLBACK_PHASE_EXIT) {
                Caliper::instance().end(s_instance->m_api_attr);
            }
        }
    }

    static void mem_alloc_callback(
        rocprofiler_callback_tracing_record_t record,
        rocprofiler_user_data_t* /* user_data */,
        void* /* callback data */
    )
    {
        if (!s_instance)
            return;
        if (record.kind != ROCPROFILER_CALLBACK_TRACING_MEMORY_ALLOCATION)
            return;

        auto* data = static_cast<rocprofiler_callback_tracing_memory_allocation_data_t*>(record.payload);

        if (record.operation == ROCPROFILER_MEMORY_ALLOCATION_ALLOCATE && data->address.ptr != nullptr) {
            Caliper c;
            c.memory_region_begin(data->address.ptr, "hip", 1, 1, &data->allocation_size);
        } else if (record.operation == ROCPROFILER_MEMORY_ALLOCATION_FREE && data->address.ptr != nullptr) {
            Caliper c;
            c.memory_region_end(data->address.ptr);
        }
    }

#ifdef CALI_ROCPROFILER_HAVE_COUNTERS
    static void dispatch_counter_config_callback(
        rocprofiler_dispatch_counting_service_data_t dispatch_data,
        rocprofiler_counter_config_id_t*             config,
        rocprofiler_user_data_t* /* user_data */,
        void* /* callback data */
    )
    {
        if (!s_instance)
            return;

        auto it = s_instance->m_counter_profile_map.find(dispatch_data.dispatch_info.agent_id.handle);
        if (it != s_instance->m_counter_profile_map.end())
            *config = it->second;
        else
            return;

        Entry e = Caliper::instance().get_path_node();
        std::lock_guard<std::mutex> g(s_instance->m_counter_dispatch_correlation_mutex);
        s_instance->m_counter_dispatch_correlation_map.emplace(dispatch_data.dispatch_info.dispatch_id, e);
    }
#endif

    void snapshot_cb(Caliper* c, SnapshotView trigger_info, SnapshotBuilder& snapshot)
    {
        auto ts = rocprofiler_timestamp_t {};
        rocprofiler_get_timestamp(&ts);

        uint64_t timestamp = static_cast<uint64_t>(ts);
        Variant  v_now(cali_make_variant_from_uint(timestamp));
        Variant  v_prev = c->exchange(m_prev_timestamp_attr, v_now);

        snapshot.append(m_host_duration_attr, cali_make_variant_from_uint(timestamp - v_prev.to_uint()));
        snapshot.append(m_host_timestamp_attr, v_now);
    }

    void post_init_cb(Caliper* c, Channel* channel)
    {
        int status = 0;
        ROCPROFILER_CALL(rocprofiler_context_is_valid(s_rocprofiler_ctx, &status));
        if (!status) {
            Log(0).stream() << channel->name() << ": rocprofiler: contexts not initialized! Skipping ROCm profiling.\n";
            return;
        }

        if (m_enable_api_callbacks) {
            channel->events().subscribe_attribute(c, m_api_attr);
            ROCPROFILER_CALL(rocprofiler_start_context(s_hip_api_ctx));
        }

        if (m_enable_activity_tracing) {
            ROCPROFILER_CALL(rocprofiler_start_context(s_rocprofiler_ctx));
            ROCPROFILER_CALL(rocprofiler_start_context(s_activity_ctx));
        }

        if (m_enable_allocation_tracing) {
            ROCPROFILER_CALL(rocprofiler_start_context(s_alloc_tracing_ctx));
        }

        if (m_enable_counters) {
            ROCPROFILER_CALL(rocprofiler_start_context(s_counter_ctx));
        }

        if (m_enable_activity_tracing || m_enable_counters) {
            channel->events().pre_flush_evt.connect([this](Caliper*, ChannelBody*, SnapshotView) { this->pre_flush_cb(); });
        }

        if (m_enable_snapshot_timestamps) {
            auto ts = rocprofiler_timestamp_t {};
            rocprofiler_get_timestamp(&ts);
            c->set(m_prev_timestamp_attr, Variant(cali_make_variant_from_uint(static_cast<uint64_t>(ts))));

            channel->events().create_thread_evt.connect(
                [this](Caliper* c, Channel*) {
                    auto ts = rocprofiler_timestamp_t {};
                    rocprofiler_get_timestamp(&ts);
                    c->set(m_prev_timestamp_attr, Variant(cali_make_variant_from_uint(static_cast<uint64_t>(ts))));
                }
            );
            channel->events().snapshot.connect(
                [this](Caliper* c, SnapshotView trigger_info, SnapshotBuilder& snapshot) {
                    this->snapshot_cb(c, trigger_info, snapshot);
                }
            );
        }
    }

    void pre_finish_cb(Caliper* c, Channel* channel)
    {
        int status = 0;
        ROCPROFILER_CALL(rocprofiler_context_is_active(s_hip_api_ctx, &status));
        if (status)
            ROCPROFILER_CALL(rocprofiler_stop_context(s_hip_api_ctx));
        ROCPROFILER_CALL(rocprofiler_context_is_active(s_rocprofiler_ctx, &status));
        if (status)
            ROCPROFILER_CALL(rocprofiler_stop_context(s_rocprofiler_ctx));
        ROCPROFILER_CALL(rocprofiler_context_is_active(s_activity_ctx, &status));
        if (status)
            ROCPROFILER_CALL(rocprofiler_stop_context(s_activity_ctx));
        ROCPROFILER_CALL(rocprofiler_context_is_active(s_counter_ctx, &status));
        if (status)
            ROCPROFILER_CALL(rocprofiler_stop_context(s_counter_ctx));

        Log(1).stream() << channel->name() << ": rocprofiler: wrote " << m_num_activity_records
            << " activity records, " << m_num_counter_records << " counter records.\n";
        if (m_failed_correlations > 0)
            Log(1).stream() << channel->name() << ": rocprofiler: " << m_failed_correlations
                << " correlation lookups failed.\n";
    }

#ifdef CALI_ROCPROFILER_HAVE_COUNTERS
    void setup_counter_profile_for_agent(Caliper* c, rocprofiler_agent_id_t agent, const std::vector<std::string>& counter_names)
    {
        std::vector<rocprofiler_counter_id_t> agent_counters;
        auto iter_counters_cb = [](rocprofiler_agent_id_t,
                                   rocprofiler_counter_id_t* counters,
                                   size_t num_counters,
                                   void* user_data)
            {
                auto* vec = static_cast<std::vector<rocprofiler_counter_id_t>*>(user_data);
                std::copy_n(counters, num_counters, std::back_inserter(*vec));
                return ROCPROFILER_STATUS_SUCCESS;
            };

        ROCPROFILER_CALL(rocprofiler_iterate_agent_supported_counters(agent, iter_counters_cb, &agent_counters));

        std::vector<rocprofiler_counter_id_t> collect_counters;
        std::vector<std::string> names = counter_names;
        for (auto counter : agent_counters) {
            rocprofiler_counter_info_v1_t info;
            ROCPROFILER_CALL(rocprofiler_query_counter_info(counter, ROCPROFILER_COUNTER_INFO_VERSION_1, &info));
            auto it = std::find(names.begin(), names.end(), std::string(info.name));
            if (it != names.end()) {
                collect_counters.push_back(counter);
                Attribute attr = c->create_attribute(std::string("rocm.").append(*it), CALI_TYPE_DOUBLE,
                    CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE | CALI_ATTR_SKIP_EVENTS);
                m_counter_attr_map.emplace(counter.handle, attr);
                names.erase(it);

                std::vector<CounterDimensionData> dim_info;
                dim_info.reserve(info.dimensions_count);
                for (std::size_t n = 0; n < info.dimensions_count; ++n) {
                    const auto* dim = info.dimensions[n];
                    Attribute dim_attr = c->create_attribute(std::string("rocm.dim.").append(dim->name),
                        CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
                    dim_info.emplace_back(CounterDimensionData { *dim, dim_attr });
                }
                m_counter_dimension_info_map.emplace(counter.handle, std::move(dim_info));
            }
        }

        if (!collect_counters.empty()) {
            rocprofiler_counter_config_id_t profile = { .handle = 0 };
            ROCPROFILER_CALL(rocprofiler_create_counter_config(agent, collect_counters.data(), collect_counters.size(), &profile));
            m_counter_profile_map.insert(std::make_pair(agent.handle, profile));
            Log(2).stream() << m_channel.name() << ": rocprofiler: Created profile of " << collect_counters.size()
                << " counter(s)\n";
        }

        for (const auto& name : names) {
            Log(0).stream() << m_channel.name() << ": rocprofiler: Counter " << name << " not found for agent " << agent.handle << "\n";
        }
    }

    void setup_counter_profiles(Caliper* c, const std::vector<std::string>& counter_names)
    {
        for (const auto &it : m_agent_info_map) {
            if (it.second->type == ROCPROFILER_AGENT_TYPE_GPU) {
                Log(2).stream() << m_channel.name() << ": rocprofiler: Setting up counters for agent "
                    << it.second->logical_node_id << " (" << it.second->name << ")\n";
                setup_counter_profile_for_agent(c, it.second->id, counter_names);
            }
        }

        ROCPROFILER_CALL(rocprofiler_configure_buffer_dispatch_counting_service(
            s_counter_ctx,
            s_activity_buf,
            dispatch_counter_config_callback,
            nullptr
        ));

        Log(1).stream() << m_channel.name() << ": rocprofiler: Created counter profiles for "
            << m_counter_profile_map.size() << " agents\n";

        m_enable_counters = !m_counter_profile_map.empty();
    }
#endif

    RocProfilerService(Caliper* c, Channel* channel) : m_channel { *channel }
    {
        auto config = services::init_config_from_spec(channel->config(), s_spec);

        m_enable_api_callbacks       = config.get("enable_api_callbacks").to_bool();
        m_enable_activity_tracing    = config.get("enable_activity_tracing").to_bool();
        m_enable_snapshot_timestamps = config.get("enable_snapshot_timestamps").to_bool();
        m_enable_allocation_tracing  = config.get("enable_allocation_tracing").to_bool();

        create_attributes(c);

        // initialize rocprofiler agents
        //

        rocprofiler_query_available_agents_cb_t iterate_agents =
            [](rocprofiler_agent_version_t, const void** agents_arr, size_t num_agents, void* usr) {
                for (size_t i = 0; i < num_agents; ++i) {
                    const auto* agent = static_cast<const rocprofiler_agent_v0_t*>(agents_arr[i]);
                    RocProfilerService* instance = static_cast<RocProfilerService*>(usr);
                    instance->m_agent_info_map.emplace(agent->id.handle, agent);
                }
                return ROCPROFILER_STATUS_SUCCESS;
            };

        ROCPROFILER_CALL(rocprofiler_query_available_agents(
            ROCPROFILER_AGENT_INFO_VERSION_0,
            iterate_agents,
            sizeof(rocprofiler_agent_t),
            this
        ));

        auto counter_names = config.get("counters").to_stringlist();
        if (!counter_names.empty()) {
#ifdef CALI_ROCPROFILER_HAVE_COUNTERS
            setup_counter_profiles(c, counter_names);
#else
            Log(0).stream() << channel->name() << ": rocprofiler: Counter collection is not supported!\n";
#endif
        }
    }

public:

    static const char* s_spec;
    static const char* s_roctracer_spec;

    static int tool_init(rocprofiler_client_finalize_t fini_func, void* tool_data)
    {
        ROCPROFILER_CALL(rocprofiler_create_context(&s_hip_api_ctx));
        ROCPROFILER_CALL(rocprofiler_create_context(&s_activity_ctx));
        ROCPROFILER_CALL(rocprofiler_create_context(&s_rocprofiler_ctx));
        ROCPROFILER_CALL(rocprofiler_create_context(&s_alloc_tracing_ctx));
        ROCPROFILER_CALL(rocprofiler_create_context(&s_counter_ctx));

        ROCPROFILER_CALL(rocprofiler_configure_callback_tracing_service(
            s_hip_api_ctx,
            ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API,
            nullptr,
            0,
            tool_api_callback,
            nullptr
        ));
        ROCPROFILER_CALL(rocprofiler_configure_callback_tracing_service(
            s_rocprofiler_ctx,
            ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
            nullptr,
            0,
            tool_api_callback,
            nullptr
        ));
        ROCPROFILER_CALL(rocprofiler_configure_callback_tracing_service(
            s_alloc_tracing_ctx,
            ROCPROFILER_CALLBACK_TRACING_MEMORY_ALLOCATION,
            nullptr,
            0,
            mem_alloc_callback,
            nullptr
        ));

        ROCPROFILER_CALL(rocprofiler_create_buffer(
            s_activity_ctx,
            1024 * 1024,
            1024 * 1024 - 8192,
            ROCPROFILER_BUFFER_POLICY_LOSSLESS,
            tool_tracing_callback,
            nullptr,
            &s_activity_buf
        ));

        // auto kernel_dispatch_cb_ops =
        //     std::array<rocprofiler_tracing_operation_t, 1>{ROCPROFILER_KERNEL_DISPATCH_COMPLETE};

        ROCPROFILER_CALL(rocprofiler_configure_buffer_tracing_service(
            s_activity_ctx,
            ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH,
            nullptr,
            0,
            s_activity_buf
        ));
        ROCPROFILER_CALL(rocprofiler_configure_buffer_tracing_service(
            s_activity_ctx,
            ROCPROFILER_BUFFER_TRACING_MEMORY_COPY,
            nullptr,
            0,
            s_activity_buf
        ));

        /*
        ROCPROFILER_CALL(
            rocprofiler_configure_buffer_tracing_service(
                activity_ctx,
                ROCPROFILER_BUFFER_TRACING_CORRELATION_ID_RETIREMENT,
                nullptr, 0, activity_buf));
*/
        auto external_corr_id_request_kinds = make_array(
            ROCPROFILER_EXTERNAL_CORRELATION_REQUEST_KERNEL_DISPATCH,
            ROCPROFILER_EXTERNAL_CORRELATION_REQUEST_MEMORY_COPY
        );

        ROCPROFILER_CALL(rocprofiler_configure_external_correlation_id_request_service(
            s_activity_ctx,
            external_corr_id_request_kinds.data(),
            external_corr_id_request_kinds.size(),
            set_external_correlation_id,
            nullptr
        ));

        // auto client_thread = rocprofiler_callback_thread_t{};
        // ROCPROFILER_CALL(
        //     rocprofiler_create_callback_thread(&client_thread));
        // ROCPROFILER_CALL(
        //     rocprofiler_assign_callback_thread(activity_buf, client_thread));

        return 0;
    }

    static void tool_fini(void* tool_data) {}

    static void register_rocprofiler(Caliper* c, Channel* channel)
    {
        if (s_instance) {
            Log(0).stream() << channel->name() << ": rocprofiler service is already active, disabling!" << std::endl;
        }

        s_instance = new RocProfilerService(c, channel);

        channel->events().post_init_evt.connect([](Caliper* c, Channel* channel) {
            s_instance->post_init_cb(c, channel);
        });
        channel->events().pre_finish_evt.connect([](Caliper* c, Channel* channel) {
            s_instance->pre_finish_cb(c, channel);
        });
        channel->events().finish_evt.connect([](Caliper* c, Channel* channel) {
            delete s_instance;
            s_instance = nullptr;
        });

        Log(1).stream() << channel->name() << ": Registered rocprofiler service."
                        << " Activity tracing is " << (s_instance->m_enable_activity_tracing ? "on" : "off")
                        << std::endl;
    }

    static void register_rocprofiler_as_roctracer(Caliper* c, Channel* channel)
    {
        // a compatibility layer to convert roctracer options into rocprofiler options

        Log(1).stream() << channel->name() << ": rocprofiler: Using roctracer compatibility layer.\n";

        auto config = services::init_config_from_spec(channel->config(), s_roctracer_spec);

        bool enable_activity_tracing = config.get("trace_activities").to_bool();
        bool enable_snapshot_timestamps =
            config.get("snapshot_duration").to_bool() || config.get("snapshot_timestamps").to_bool();

        channel->config().set("CALI_ROCPROFILER_ENABLE_ACTIVITY_TRACING", enable_activity_tracing ? "true" : "false");
        channel->config().set(
            "CALI_ROCPROFILER_ENABLE_SNAPSHOT_TIMESTAMPS",
            enable_snapshot_timestamps ? "true" : "false"
        );

        register_rocprofiler(c, channel);
    }
};

RocProfilerService* RocProfilerService::s_instance = nullptr;

rocprofiler_context_id_t RocProfilerService::s_hip_api_ctx       = {};
rocprofiler_context_id_t RocProfilerService::s_activity_ctx      = {};
rocprofiler_context_id_t RocProfilerService::s_rocprofiler_ctx   = {};
rocprofiler_context_id_t RocProfilerService::s_alloc_tracing_ctx = {};
rocprofiler_context_id_t RocProfilerService::s_counter_ctx       = {};

rocprofiler_buffer_id_t RocProfilerService::s_activity_buf = {};

const char* RocProfilerService::s_spec = R"json(
{
 "name": "rocprofiler",
 "description": "Record ROCm API and GPU activities using rocprofiler-sdk",
 "config":
 [
  { "name": "enable_api_callbacks",
    "type": "bool",
    "description": "Enable HIP API interception callbacks",
    "value": "true"
  },
  { "name": "enable_activity_tracing",
    "type": "bool",
    "description": "Enable ROCm GPU activity tracing",
    "value": "false"
  },
  { "name": "enable_snapshot_timestamps",
    "type": "bool",
    "description": "Record host-side timestamps and durations with rocprofiler",
    "value": "false"
  },
  { "name": "enable_allocation_tracing",
    "type": "bool",
    "description": "Trace HIP memory allocations",
    "value": "false"
  },
  { "name": "counters",
    "type": "string",
    "description": "List of performance counters to collect"
  }
 ]
}
)json";

const char* RocProfilerService::s_roctracer_spec = R"json(
{
 "name": "roctracer",
 "description": "roctracer compatibility layer for rocprofiler service (deprecated)",
 "config":
 [
  { "name": "trace_activities",
    "type": "bool",
    "description": "Enable ROCm GPU activity tracing",
    "value": "true"
  },
  { "name": "record_kernel_names",
    "type": "bool",
    "description": "Record kernel names when activity tracing is enabled",
    "value": "false"
  },
  { "name": "snapshot_duration",
    "type": "bool",
    "description": "Record duration of host-side activities using ROCm timestamps",
    "value": "false"
  },
  { "name": "snapshot_timestamps",
    "type": "bool",
    "description": "Record host-side timestamps with ROCm",
    "value": "false"
   }
  ]
}
)json";

} // namespace

extern "C"
{

rocprofiler_tool_configure_result_t* rocprofiler_configure(
    uint32_t                 version,
    const char*              runtime_version,
    uint32_t                 priority,
    rocprofiler_client_id_t* id
)
{
    id->name = "Caliper";

    static auto cfg = rocprofiler_tool_configure_result_t { sizeof(rocprofiler_tool_configure_result_t),
                                                            &RocProfilerService::tool_init,
                                                            &RocProfilerService::tool_fini,
                                                            nullptr };

    return &cfg;
}

} // extern "C"

namespace cali
{

CaliperService rocprofiler_service { ::RocProfilerService::s_spec, ::RocProfilerService::register_rocprofiler };

CaliperService roctracer_service { ::RocProfilerService::s_roctracer_spec,
                                   ::RocProfilerService::register_rocprofiler_as_roctracer };

} // namespace cali
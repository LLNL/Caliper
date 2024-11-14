// Copyright (c) 2015-2024, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include "../../common/util/unitfmt.h"
#include "../../common/util/demangle.h"

#include <rocprofiler-sdk/rocprofiler.h>
#include <rocprofiler-sdk/registration.h>

#include <array>
#include <mutex>
#include <sstream>

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

    Attribute m_activity_start_attr;
    Attribute m_activity_end_attr;
    Attribute m_activity_name_attr;
    Attribute m_activity_bytes_attr;
    Attribute m_activity_device_id_attr;
    Attribute m_activity_queue_id_attr;
    Attribute m_activity_duration_attr;
    Attribute m_src_agent_attr;
    Attribute m_dst_agent_attr;
    Attribute m_agent_attr;

    Attribute m_flush_region_attr;

    bool m_enable_api_callbacks       = false;
    bool m_enable_activity_tracing    = false;
    bool m_enable_snapshot_timestamps = false;

    unsigned m_num_activity_records = 0;

    std::map<uint64_t, std::string> m_kernel_info;
    std::mutex                      m_kernel_info_mutex;

    Channel m_channel;

    static RocProfilerService* s_instance;

    static rocprofiler_context_id_t hip_api_ctx;
    static rocprofiler_context_id_t activity_ctx;
    static rocprofiler_context_id_t rocprofiler_ctx;

    static rocprofiler_buffer_id_t activity_buf;

    static std::map<uint64_t, const rocprofiler_agent_t*> s_agents;

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

        m_host_timestamp_attr = c->create_attribute(
            "rocm.host.timestamp",
            CALI_TYPE_UINT,
            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS
        );
        m_host_duration_attr = c->create_attribute(
            "rocm.host.duration",
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

        m_flush_region_attr =
            c->create_attribute("rocprofiler.flush", CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_DEFAULT);
    }

    void update_kernel_info(uint64_t kernel_id, const std::string& name)
    {
        std::lock_guard<std::mutex> g(m_kernel_info_mutex);

        m_kernel_info.emplace(kernel_id, name);
    }

    const char* get_kernel_name(uint64_t kernel_id)
    {
        std::lock_guard<std::mutex> g(m_kernel_info_mutex);

        auto it = m_kernel_info.find(kernel_id);
        return it != m_kernel_info.end() ? it->second.c_str() : nullptr;
    }

    void pre_flush_cb()
    {
        if (activity_buf.handle > 0)
            ROCPROFILER_CALL(rocprofiler_flush_buffer(activity_buf));
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
        Caliper c;
        c.begin(s_instance->m_flush_region_attr, Variant("ROCPROFILER FLUSH"));

        for (size_t i = 0; i < num_headers; ++i) {
            auto* header = headers[i];

            if (header->category == ROCPROFILER_BUFFER_CATEGORY_TRACING
                && header->kind == ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH) {

                auto* record = static_cast<rocprofiler_buffer_tracing_kernel_dispatch_record_t*>(header->payload);

                const Attribute attr[] = { s_instance->m_activity_name_attr, s_instance->m_activity_start_attr,
                                           s_instance->m_activity_end_attr,  s_instance->m_activity_duration_attr,
                                           s_instance->m_kernel_name_attr,   s_instance->m_agent_attr };

                const char* activity_name = nullptr;
                uint64_t    len;
                ROCPROFILER_CALL(rocprofiler_query_buffer_tracing_kind_operation_name(
                    record->kind,
                    record->operation,
                    &activity_name,
                    &len
                ));

                const char* kernel_name = s_instance->get_kernel_name(record->dispatch_info.kernel_id);

                uint64_t agent = s_agents.at(record->dispatch_info.agent_id.handle)->logical_node_id;

                const Variant data[] = {
                    Variant(CALI_TYPE_STRING, activity_name, len),
                    Variant(cali_make_variant_from_uint(record->start_timestamp)),
                    Variant(cali_make_variant_from_uint(record->end_timestamp)),
                    Variant(cali_make_variant_from_uint(record->end_timestamp - record->start_timestamp)),
                    Variant(kernel_name),
                    Variant(cali_make_variant_from_uint(agent))
                };

                cali::Node* correlation = static_cast<cali::Node*>(record->correlation_id.external.ptr);

                FixedSizeSnapshotRecord<6> snapshot;
                c.make_record(6, attr, data, snapshot.builder(), correlation);
                s_instance->m_channel.events()
                    .process_snapshot(&c, &s_instance->m_channel, SnapshotView(), snapshot.view());

                ++s_instance->m_num_activity_records;
            } else if (header->category == ROCPROFILER_BUFFER_CATEGORY_TRACING && header->kind == ROCPROFILER_BUFFER_TRACING_MEMORY_COPY) {

                auto* record = static_cast<rocprofiler_buffer_tracing_memory_copy_record_t*>(header->payload);

                const Attribute attr[] = { s_instance->m_activity_name_attr, s_instance->m_activity_start_attr,
                                           s_instance->m_activity_end_attr,  s_instance->m_activity_duration_attr,
                                           s_instance->m_src_agent_attr,     s_instance->m_dst_agent_attr };

                const char* activity_name = nullptr;
                uint64_t    len;
                ROCPROFILER_CALL(rocprofiler_query_buffer_tracing_kind_operation_name(
                    record->kind,
                    record->operation,
                    &activity_name,
                    &len
                ));

                uint64_t src_agent = s_agents.at(record->src_agent_id.handle)->logical_node_id;
                uint64_t dst_agent = s_agents.at(record->dst_agent_id.handle)->logical_node_id;

                const Variant data[] = {
                    Variant(CALI_TYPE_STRING, activity_name, len),
                    Variant(cali_make_variant_from_uint(record->start_timestamp)),
                    Variant(cali_make_variant_from_uint(record->end_timestamp)),
                    Variant(cali_make_variant_from_uint(record->end_timestamp - record->start_timestamp)),
                    Variant(cali_make_variant_from_uint(src_agent)),
                    Variant(cali_make_variant_from_uint(dst_agent))
                };

                cali::Node* correlation = static_cast<cali::Node*>(record->correlation_id.external.ptr);

                FixedSizeSnapshotRecord<6> snapshot;
                c.make_record(6, attr, data, snapshot.builder(), correlation);
                s_instance->m_channel.events()
                    .process_snapshot(&c, &s_instance->m_channel, SnapshotView(), snapshot.view());

                ++s_instance->m_num_activity_records;
            }
        }

        c.end(s_instance->m_flush_region_attr);
    }

    static void tool_api_cb(
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
                Caliper c;
                if (name)
                    c.begin(s_instance->m_api_attr, Variant(CALI_TYPE_STRING, name, len));
            } else if (record.phase == ROCPROFILER_CALLBACK_PHASE_EXIT) {
                Caliper c;
                c.end(s_instance->m_api_attr);
            }
        }
    }

    void snapshot_cb(Caliper* c, Channel* channel, SnapshotView trigger_info, SnapshotBuilder& snapshot)
    {
        auto ts = rocprofiler_timestamp_t {};
        rocprofiler_get_timestamp(&ts);

        uint64_t timestamp = static_cast<uint64_t>(ts);
        Variant  v_now(cali_make_variant_from_uint(timestamp));
        Variant  v_prev = c->exchange(m_host_timestamp_attr, v_now);

        snapshot.append(Entry(m_host_duration_attr, cali_make_variant_from_uint(timestamp - v_prev.to_uint())));
    }

    void post_init_cb(Caliper* c, Channel* channel)
    {
        if (m_enable_api_callbacks) {
            channel->events().subscribe_attribute(c, channel, m_api_attr);
            ROCPROFILER_CALL(rocprofiler_start_context(hip_api_ctx));
        }

        if (m_enable_activity_tracing) {
            ROCPROFILER_CALL(rocprofiler_start_context(rocprofiler_ctx));
            ROCPROFILER_CALL(rocprofiler_start_context(activity_ctx));

            channel->events().pre_flush_evt.connect([this](Caliper*, Channel*, SnapshotView) { this->pre_flush_cb(); });
        }

        if (m_enable_snapshot_timestamps) {
            auto ts = rocprofiler_timestamp_t {};
            rocprofiler_get_timestamp(&ts);
            c->set(m_host_timestamp_attr, Variant(cali_make_variant_from_uint(static_cast<uint64_t>(ts))));

            channel->events().snapshot.connect(
                [this](Caliper* c, Channel* channel, SnapshotView trigger_info, SnapshotBuilder& snapshot) {
                    this->snapshot_cb(c, channel, trigger_info, snapshot);
                }
            );
        }
    }

    void pre_finish_cb(Caliper* c, Channel* channel)
    {
        int status = 0;
        ROCPROFILER_CALL(rocprofiler_context_is_active(hip_api_ctx, &status));
        if (status)
            ROCPROFILER_CALL(rocprofiler_stop_context(hip_api_ctx));
        ROCPROFILER_CALL(rocprofiler_context_is_active(rocprofiler_ctx, &status));
        if (status)
            ROCPROFILER_CALL(rocprofiler_stop_context(rocprofiler_ctx));
        ROCPROFILER_CALL(rocprofiler_context_is_active(activity_ctx, &status));
        if (status)
            ROCPROFILER_CALL(rocprofiler_stop_context(activity_ctx));

        Log(1).stream() << channel->name() << ": rocprofiler: wrote " << m_num_activity_records
                        << " activity records\n";
    }

    RocProfilerService(Caliper* c, Channel* channel) : m_channel { *channel }
    {
        auto config = services::init_config_from_spec(channel->config(), s_spec);

        m_enable_api_callbacks       = config.get("enable_api_callbacks").to_bool();
        m_enable_activity_tracing    = config.get("enable_activity_tracing").to_bool();
        m_enable_snapshot_timestamps = config.get("enable_snapshot_timestamps").to_bool();

        create_attributes(c);
    }

public:

    static const char* s_spec;
    static const char* s_roctracer_spec;

    static int tool_init(rocprofiler_client_finalize_t fini_func, void* tool_data)
    {
        ROCPROFILER_CALL(rocprofiler_create_context(&hip_api_ctx));
        ROCPROFILER_CALL(rocprofiler_create_context(&activity_ctx));
        ROCPROFILER_CALL(rocprofiler_create_context(&rocprofiler_ctx));

        ROCPROFILER_CALL(rocprofiler_configure_callback_tracing_service(
            hip_api_ctx,
            ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API,
            nullptr,
            0,
            tool_api_cb,
            nullptr
        ));
        ROCPROFILER_CALL(rocprofiler_configure_callback_tracing_service(
            rocprofiler_ctx,
            ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
            nullptr,
            0,
            tool_api_cb,
            nullptr
        ));

        ROCPROFILER_CALL(rocprofiler_create_buffer(
            activity_ctx,
            16 * 1024 * 1024,
            15 * 1024 * 1024,
            ROCPROFILER_BUFFER_POLICY_LOSSLESS,
            tool_tracing_callback,
            nullptr,
            &activity_buf
        ));

        // auto kernel_dispatch_cb_ops =
        //     std::array<rocprofiler_tracing_operation_t, 1>{ROCPROFILER_KERNEL_DISPATCH_COMPLETE};

        ROCPROFILER_CALL(rocprofiler_configure_buffer_tracing_service(
            activity_ctx,
            ROCPROFILER_BUFFER_TRACING_KERNEL_DISPATCH,
            nullptr,
            0,
            activity_buf
        ));
        ROCPROFILER_CALL(rocprofiler_configure_buffer_tracing_service(
            activity_ctx,
            ROCPROFILER_BUFFER_TRACING_MEMORY_COPY,
            nullptr,
            0,
            activity_buf
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
            activity_ctx,
            external_corr_id_request_kinds.data(),
            external_corr_id_request_kinds.size(),
            set_external_correlation_id,
            nullptr
        ));

        rocprofiler_query_available_agents_cb_t iterate_agents =
            [](rocprofiler_agent_version_t, const void** agents_arr, size_t num_agents, void*) {
                for (size_t i = 0; i < num_agents; ++i) {
                    const auto* agent_v = static_cast<const rocprofiler_agent_v0_t*>(agents_arr[i]);
                    s_agents.emplace(agent_v->id.handle, agent_v);
                }
                return ROCPROFILER_STATUS_SUCCESS;
            };

        ROCPROFILER_CALL(rocprofiler_query_available_agents(
            ROCPROFILER_AGENT_INFO_VERSION_0,
            iterate_agents,
            sizeof(rocprofiler_agent_t),
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

rocprofiler_context_id_t RocProfilerService::hip_api_ctx     = {};
rocprofiler_context_id_t RocProfilerService::activity_ctx    = {};
rocprofiler_context_id_t RocProfilerService::rocprofiler_ctx = {};

rocprofiler_buffer_id_t RocProfilerService::activity_buf = {};

std::map<uint64_t, const rocprofiler_agent_t*> RocProfilerService::s_agents;

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
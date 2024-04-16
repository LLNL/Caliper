// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
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

#define ROCPROFILER_CALL(result)                                                              \
    {                                                                                              \
        rocprofiler_status_t CHECKSTATUS = result;                                                 \
        if(CHECKSTATUS != ROCPROFILER_STATUS_SUCCESS)                                              \
        {                                                                                          \
            std::string status_msg = rocprofiler_get_status_string(CHECKSTATUS);                   \
            std::cerr << "[" #result "][" << __FILE__ << ":" << __LINE__ << "] "            \
                      << " failed with error code " << CHECKSTATUS << ": " << status_msg           \
                      << std::endl;                                                                \
            std::stringstream errmsg{};                                                            \
            errmsg << "[" #result "][" << __FILE__ << ":" << __LINE__ << "] failure ("  \
                   << status_msg << ")";                                                           \
            throw std::runtime_error(errmsg.str());                                                \
        }                                                                                          \
    }


namespace
{

class RocProfilerService
{
    Attribute m_api_attr;

    Attribute m_kernel_name_attr;

    Attribute m_activity_start_attr;
    Attribute m_activity_end_attr;
    Attribute m_activity_name_attr;
    Attribute m_activity_bytes_attr;
    Attribute m_activity_device_id_attr;
    Attribute m_activity_queue_id_attr;
    Attribute m_activity_duration_attr;

    Attribute m_flush_region_attr;

    bool      m_enable_activity_tracing = false;

    struct kernel_info_t {
        std::string formatted_name;
    };

    std::map<uint64_t, std::string> m_kernel_info;
    std::mutex m_kernel_info_mutex;

    static RocProfilerService* s_instance;

    static rocprofiler_context_id_t hip_api_ctx;
    static rocprofiler_context_id_t activity_ctx;
    static rocprofiler_context_id_t hsa_api_ctx;
    static rocprofiler_context_id_t rocprofiler_ctx;

    static rocprofiler_buffer_id_t activity_buf;
    static rocprofiler_buffer_id_t correlation_id_retire_buf;

    void create_attributes(Caliper* c)
    {
        m_api_attr =
            c->create_attribute("rocm.api", CALI_TYPE_STRING, CALI_ATTR_NESTED);

        m_activity_start_attr =
            c->create_attribute("rocm.starttime", CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
        m_activity_end_attr =
            c->create_attribute("rocm.endtime", CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);

        m_activity_duration_attr =
            c->create_attribute("rocm.activity.duration", CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE     |
                                CALI_ATTR_SKIP_EVENTS |
                                CALI_ATTR_AGGREGATABLE);

        m_activity_name_attr =
            c->create_attribute("rocm.activity", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        m_activity_queue_id_attr =
            c->create_attribute("rocm.activity.queue", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        m_activity_device_id_attr =
            c->create_attribute("rocm.activity.device", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        m_activity_bytes_attr =
            c->create_attribute("rocm.activity.bytes", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        m_kernel_name_attr =
            c->create_attribute("rocm.kernel.name", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);

        m_flush_region_attr =
            c->create_attribute("roctracer.flush", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
    }

    void update_kernel_info(uint64_t kernel_id, const std::string& name) {
        std::lock_guard<std::mutex>
            g(m_kernel_info_mutex);

        m_kernel_info.emplace(kernel_id, name);
        Log(1).stream() << "Kernel " << kernel_id << ": " << name << "\n";
    }

    static void tool_tracing_cb(rocprofiler_callback_tracing_record_t record,
                           rocprofiler_user_data_t* user_data,
                           void* /* callback_data */)
    {
        if (!s_instance)
            return;

        if (record.kind == ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT) {
            if (record.operation == ROCPROFILER_CODE_OBJECT_DEVICE_KERNEL_SYMBOL_REGISTER &&
                record.phase == ROCPROFILER_CALLBACK_PHASE_LOAD) {
                auto *data = static_cast<rocprofiler_callback_tracing_code_object_kernel_symbol_register_data_t*>(record.payload);
                s_instance->update_kernel_info(data->kernel_id, util::demangle(data->kernel_name));
            }
        } else if(record.kind == ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH) {
            auto* data =
                static_cast<rocprofiler_callback_tracing_kernel_dispatch_data_t*>(record.payload);

            auto name = s_instance->m_kernel_info.at(data->dispatch_info.kernel_id);
            uint64_t start = data->start_timestamp;
            uint64_t end = data->end_timestamp;
            Log(1).stream() << "Kernel: " << name << ": " << end-start << "nsec \n";
            // push record
        } else {
            if (record.phase == ROCPROFILER_CALLBACK_PHASE_ENTER) {
                const char* name = nullptr;
                uint64_t len = 0;
                ROCPROFILER_CALL(
                    rocprofiler_query_callback_tracing_kind_operation_name(
                        record.kind, record.operation, &name, &len));
                Caliper c;
                if (name)
                    c.begin(s_instance->m_api_attr, Variant(CALI_TYPE_STRING, name, len));
            } else if (record.phase == ROCPROFILER_CALLBACK_PHASE_EXIT) {
                Caliper c;
                c.end(s_instance->m_api_attr);
            }
        }
    }

    void post_init_cb(Caliper* c, Channel* channel) {
        ROCPROFILER_CALL(rocprofiler_start_context(hip_api_ctx));

        if (m_enable_activity_tracing) {
            ROCPROFILER_CALL(rocprofiler_start_context(rocprofiler_ctx));
            ROCPROFILER_CALL(rocprofiler_start_context(activity_ctx));
        }
    }

    void pre_finish_cb(Caliper* c, Channel* channel) {
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
    }

    RocProfilerService(Caliper* c, Channel* channel)
    {
        auto config = services::init_config_from_spec(channel->config(), s_spec);

        m_enable_activity_tracing = config.get("trace_activities").to_bool();

        create_attributes(c);
    }

public:

    static const char* s_spec;

    static int tool_init(rocprofiler_client_finalize_t fini_func, void* tool_data) {
        ROCPROFILER_CALL(rocprofiler_create_context(&hip_api_ctx));
        ROCPROFILER_CALL(rocprofiler_create_context(&activity_ctx));
        ROCPROFILER_CALL(rocprofiler_create_context(&hsa_api_ctx));
        ROCPROFILER_CALL(rocprofiler_create_context(&rocprofiler_ctx));

        ROCPROFILER_CALL(
            rocprofiler_configure_callback_tracing_service(
                hip_api_ctx,
                ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API,
                nullptr, 0, tool_tracing_cb, nullptr));
        ROCPROFILER_CALL(
            rocprofiler_configure_callback_tracing_service(
                rocprofiler_ctx,
                ROCPROFILER_CALLBACK_TRACING_CODE_OBJECT,
                nullptr, 0, tool_tracing_cb, nullptr));

        auto kernel_dispatch_cb_ops =
            std::array<rocprofiler_tracing_operation_t, 1>{ROCPROFILER_KERNEL_DISPATCH_COMPLETE};

        ROCPROFILER_CALL(
            rocprofiler_configure_callback_tracing_service(activity_ctx,
                                                       ROCPROFILER_CALLBACK_TRACING_KERNEL_DISPATCH,
                                                       kernel_dispatch_cb_ops.data(),
                                                       kernel_dispatch_cb_ops.size(),
                                                       tool_tracing_cb,
                                                       nullptr));
        return 0;
    }

    static void tool_fini(void* tool_data)
    { }

    static void register_rocprofiler(Caliper* c, Channel* channel) {
        if (s_instance) {
            Log(0).stream() << channel->name()
                            << ": roctracer service is already active, disabling!"
                            << std::endl;
        }

        s_instance = new RocProfilerService(c, channel);

        channel->events().post_init_evt.connect(
            [](Caliper* c, Channel* channel){
                s_instance->post_init_cb(c, channel);
            });
        channel->events().pre_finish_evt.connect(
            [](Caliper* c, Channel* channel){
                s_instance->pre_finish_cb(c, channel);
            });
        channel->events().finish_evt.connect(
            [](Caliper* c, Channel* channel){
                delete s_instance;
                s_instance = nullptr;
            });

        Log(1).stream() << channel->name()
                        << ": Registered roctracer service."
                        << " Activity tracing is "
                        << (s_instance->m_enable_activity_tracing ? "on" : "off")
                        << std::endl;
    }
};

RocProfilerService* RocProfilerService::s_instance = nullptr;

rocprofiler_context_id_t RocProfilerService::hip_api_ctx = {};
rocprofiler_context_id_t RocProfilerService::activity_ctx = {};
rocprofiler_context_id_t RocProfilerService::hsa_api_ctx = {};
rocprofiler_context_id_t RocProfilerService::rocprofiler_ctx = {};

rocprofiler_buffer_id_t RocProfilerService::activity_buf = {};
rocprofiler_buffer_id_t RocProfilerService::correlation_id_retire_buf = {};

const char* RocProfilerService::s_spec = R"json(
{   "name": "rocprofiler",
    "description": "Record ROCm API and GPU activities",
    "config": [
        {   "name": "trace_activities",
            "type": "bool",
            "description": "Enable ROCm GPU activity tracing",
            "value": "true"
        },
        {   "name": "record_kernel_names",
            "type": "bool",
            "description": "Record kernel names when activity tracing is enabled",
            "value": "false"
        },
        {   "name": "snapshot_duration",
            "type": "bool",
            "description": "Record duration of host-side activities using ROCm timestamps",
            "value": "false"
        },
        {   "name": "snapshot_timestamps",
            "type": "bool",
            "description": "Record host-side timestamps with ROCm",
            "value": "false"
        }
    ]
}
)json";

}

extern "C"
{

rocprofiler_tool_configure_result_t*
rocprofiler_configure(uint32_t                 version,
                      const char*              runtime_version,
                      uint32_t                 priority,
                      rocprofiler_client_id_t* id)
{
    static auto cfg =
        rocprofiler_tool_configure_result_t {
            sizeof(rocprofiler_tool_configure_result_t),
            &RocProfilerService::tool_init,
            &RocProfilerService::tool_fini,
            nullptr
        };

    return &cfg;
}

} // extern "C"

namespace cali
{

CaliperService rocprofiler_service { ::RocProfilerService::s_spec, ::RocProfilerService::register_rocprofiler };

}
// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/c-util/unitfmt.h"

#include "../../common/util/demangle.h"

#include <roctracer.h>
#include <roctracer_hip.h>
#include <roctracer_ext.h>

#include <mutex>
#include <map>

using namespace cali;

namespace
{

class RocTracerService {
    Attribute m_api_attr;

    Attribute m_activity_start_attr;
    Attribute m_activity_end_attr;
    Attribute m_activity_duration_attr;
    Attribute m_activity_name_attr;
    Attribute m_activity_queue_id_attr;
    Attribute m_activity_device_id_attr;
    Attribute m_activity_bytes_attr;
    Attribute m_kernel_name_attr;

    Attribute m_host_starttime_attr;
    Attribute m_host_duration_attr;
    Attribute m_host_timestamp_attr;

    Attribute m_flush_region_attr;

    unsigned  m_num_records;
    unsigned  m_num_flushed;
    unsigned  m_num_flushes;

    unsigned  m_num_correlations_stored;
    unsigned  m_num_correlations_found;
    unsigned  m_num_correlations_missed;

    std::mutex m_correlation_map_mutex;
    std::map<uint64_t, cali::Node*> m_correlation_map;

    roctracer_pool_t* m_roctracer_pool;

    Channel*  m_channel;

    bool      m_enable_tracing;
    bool      m_record_names;
    bool      m_record_host_duration;
    bool      m_record_host_timestamp;

    static RocTracerService* s_instance;

    void create_callback_attributes(Caliper* c) {
        Attribute subs_attr = c->get_attribute("subscription_event");
        Variant v_true(true);

        m_api_attr =
            c->create_attribute("rocm.api", CALI_TYPE_STRING,
                                CALI_ATTR_NESTED,
                                1, &subs_attr, &v_true);
    }

    void create_activity_attributes(Caliper* c) {
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

    void create_host_attributes(Caliper* c) {
        m_host_starttime_attr =
            c->create_attribute("rocm.host.starttime", CALI_TYPE_UINT,
                                CALI_ATTR_SCOPE_PROCESS |
                                CALI_ATTR_SKIP_EVENTS);

        if (!(m_record_host_duration || m_record_host_timestamp))
            return;

        int hide_offset = m_record_host_timestamp ? 0 : CALI_ATTR_HIDDEN;

        m_host_timestamp_attr =
            c->create_attribute("rocm.host.timestamp", CALI_TYPE_UINT,
                                CALI_ATTR_SCOPE_THREAD |
                                CALI_ATTR_ASVALUE      |
                                CALI_ATTR_SKIP_EVENTS  |
                                hide_offset);

        if (m_record_host_duration) {
            m_host_duration_attr =
                c->create_attribute("rocm.host.duration", CALI_TYPE_UINT,
                                    CALI_ATTR_SCOPE_THREAD |
                                    CALI_ATTR_ASVALUE      |
                                    CALI_ATTR_SKIP_EVENTS  |
                                    CALI_ATTR_AGGREGATABLE);
        }
    }

    void subscribe_attributes(Caliper* c, Channel* channel) {
        channel->events().subscribe_attribute(c, channel, m_api_attr);
    }

    void push_correlation(uint64_t id, cali::Node* node) {
        std::lock_guard<std::mutex>
            g(m_correlation_map_mutex);

        m_correlation_map[id] = node;
    }

    cali::Node* pop_correlation(uint64_t id) {
        cali::Node* ret = nullptr;

        std::lock_guard<std::mutex>
            g(m_correlation_map_mutex);

        auto it = m_correlation_map.find(id);

        if (it != m_correlation_map.end()) {
            ret = it->second;
            m_correlation_map.erase(it);
        }

        return ret;
    }

    static void hip_api_callback(uint32_t domain, uint32_t cid, const void* callback_data, void* arg)
    {
        // skip unneeded callbacks
        if (cid == HIP_API_ID___hipPushCallConfiguration ||
            cid == HIP_API_ID___hipPopCallConfiguration)
            return;

        auto instance = static_cast<RocTracerService*>(arg);
        auto data = static_cast<const hip_api_data_t*>(callback_data);
        Caliper c;

        if (data->phase == ACTIVITY_API_PHASE_ENTER) {
            c.begin(instance->m_api_attr, Variant(roctracer_op_string(ACTIVITY_DOMAIN_HIP_API, cid, 0)));

            if (instance->m_enable_tracing) {
                //   When tracing, store a correlation id with the kernel name and the
                // current region context. We only store correlation IDs for the subset
                // of calls that produce activities.
                std::string kernel;
                cali::Node* node = nullptr;

                switch (cid) {
                    case HIP_API_ID_hipLaunchKernel:
                    case HIP_API_ID_hipExtLaunchKernel:
                    {
                        Entry e = c.get(instance->m_api_attr);
                        if (e.is_reference())
                            node = e.node();
                        if (instance->m_record_names) {
                            kernel = hipKernelNameRefByPtr(data->args.hipLaunchKernel.function_address,
                                        data->args.hipLaunchKernel.stream);
                        }
                    }
                    break;
                    case HIP_API_ID_hipModuleLaunchKernel:
                    case HIP_API_ID_hipExtModuleLaunchKernel:
                    case HIP_API_ID_hipHccModuleLaunchKernel:
                    {
                        Entry e = c.get(instance->m_api_attr);
                        if (e.is_reference())
                            node = e.node();
                        if (instance->m_record_names) {
                            kernel = hipKernelNameRef(data->args.hipExtModuleLaunchKernel.f);
                        }
                    }
                    break;
                    case HIP_API_ID_hipMemcpy:
                    case HIP_API_ID_hipMemcpy2D:
                    case HIP_API_ID_hipMemcpy2DAsync:
                    case HIP_API_ID_hipMemcpy2DFromArray:
                    case HIP_API_ID_hipMemcpy2DFromArrayAsync:
                    case HIP_API_ID_hipMemcpy2DToArray:
                    case HIP_API_ID_hipMemcpy2DToArrayAsync:
                    case HIP_API_ID_hipMemcpy3D:
                    case HIP_API_ID_hipMemcpy3DAsync:
                    case HIP_API_ID_hipMemcpyAsync:
                    case HIP_API_ID_hipMemcpyAtoH:
                    case HIP_API_ID_hipMemcpyDtoD:
                    case HIP_API_ID_hipMemcpyDtoDAsync:
                    case HIP_API_ID_hipMemcpyDtoH:
                    case HIP_API_ID_hipMemcpyDtoHAsync:
                    case HIP_API_ID_hipMemcpyFromArray:
                    case HIP_API_ID_hipMemcpyFromSymbol:
                    case HIP_API_ID_hipMemcpyFromSymbolAsync:
                    case HIP_API_ID_hipMemcpyHtoA:
                    case HIP_API_ID_hipMemcpyHtoD:
                    case HIP_API_ID_hipMemcpyHtoDAsync:
                    case HIP_API_ID_hipMemcpyParam2D:
                    case HIP_API_ID_hipMemcpyParam2DAsync:
                    case HIP_API_ID_hipMemcpyPeer:
                    case HIP_API_ID_hipMemcpyPeerAsync:
                    case HIP_API_ID_hipMemcpyToArray:
                    case HIP_API_ID_hipMemcpyToSymbol:
                    case HIP_API_ID_hipMemcpyToSymbolAsync:
                    case HIP_API_ID_hipMemcpyWithStream:
                    case HIP_API_ID_hipMemset:
                    case HIP_API_ID_hipMemset2D:
                    case HIP_API_ID_hipMemset2DAsync:
                    case HIP_API_ID_hipMemset3D:
                    case HIP_API_ID_hipMemset3DAsync:
                    case HIP_API_ID_hipMemsetAsync:
                    case HIP_API_ID_hipMemsetD16:
                    case HIP_API_ID_hipMemsetD32:
                    case HIP_API_ID_hipMemsetD32Async:
                    case HIP_API_ID_hipMemsetD8:
                    case HIP_API_ID_hipMemsetD8Async:
                    {
                        Entry e = c.get(instance->m_api_attr);
                        if (e.is_reference())
                            node = e.node();
                    }
                    default:
                    break;
                }

                if (!kernel.empty()) {
                    kernel = util::demangle(kernel.c_str());
                    node = c.make_tree_entry(instance->m_kernel_name_attr,
                                Variant(kernel.c_str()),
                                node);
                }

                if (node) {
                    instance->push_correlation(data->correlation_id, node);
                    ++instance->m_num_correlations_stored;
                }
            }
        } else {
            c.end(instance->m_api_attr);
        }
    }

    unsigned flush_record(Caliper* c, const roctracer_record_t* record) {
        unsigned num_records = 0;

        if (record->domain == ACTIVITY_DOMAIN_HIP_OPS || record->domain == ACTIVITY_DOMAIN_HCC_OPS) {
            Attribute attr[7] = {
                m_activity_name_attr,
                m_activity_start_attr,
                m_activity_end_attr,
                m_activity_duration_attr,
                m_activity_device_id_attr,
                m_activity_queue_id_attr,
                m_activity_bytes_attr
            };
            Variant data[7] = {
                Variant(roctracer_op_string(record->domain, record->op, record->kind)),
                Variant(cali_make_variant_from_uint(record->begin_ns)),
                Variant(cali_make_variant_from_uint(record->end_ns)),
                Variant(cali_make_variant_from_uint(record->end_ns - record->begin_ns)),
                Variant(cali_make_variant_from_uint(record->device_id)),
                Variant(cali_make_variant_from_uint(record->queue_id)),
                Variant()
            };

            size_t num = 6;

            if (record->op == HIP_OP_ID_COPY)
                data[num++] = Variant(cali_make_variant_from_uint(record->bytes));

            cali::Node* parent = pop_correlation(record->correlation_id);

            if (parent) {
                ++m_num_correlations_found;
            } else {
                ++m_num_correlations_missed;
            }

            FixedSizeSnapshotRecord<8> snapshot;
            c->make_record(num, attr, data, snapshot.builder(), parent);
            m_channel->events().process_snapshot(c, m_channel, SnapshotView(), snapshot.view());

            ++num_records;
        }

        return num_records;
    }

    void flush_activity_records(Caliper* c, const char* begin, const char* end) {
        c->begin(m_flush_region_attr, Variant("ROCTRACER FLUSH"));

        unsigned num_flushed = 0;
        unsigned num_records = 0;

        const roctracer_record_t* record =
            reinterpret_cast<const roctracer_record_t*>(begin);
        const roctracer_record_t* end_record =
            reinterpret_cast<const roctracer_record_t*>(end);

        while (record < end_record) {
            num_flushed += flush_record(c, record);

            ++num_records;
            if (roctracer_next_record(record, &record) != 0)
                break;
        }

        if (Log::verbosity() >= 2) {
            Log(2).stream() << m_channel->name() << ": roctracer: Flushed "
                            << num_records << " records ("
                            << num_flushed << " flushed, "
                            << num_records - num_flushed << " skipped).\n";
        }

        m_num_flushed += num_flushed;
        m_num_records += num_records;
        m_num_flushes++;

        c->end(m_flush_region_attr);
    }

    void pre_flush_cb() {
        roctracer_flush_activity_expl(m_roctracer_pool);
    }

    void snapshot_cb(Caliper* c, Channel* channel, int scopes, const SnapshotView, SnapshotBuilder& snapshot) {
        uint64_t timestamp = 0;
        roctracer_get_timestamp(&timestamp);

        Variant  v_now(cali_make_variant_from_uint(timestamp));
        Variant  v_prev = c->exchange(m_host_timestamp_attr, v_now);

        if (m_record_host_duration)
            snapshot.append(Entry(m_host_duration_attr,
                                  Variant(cali_make_variant_from_uint(timestamp - v_prev.to_uint()))));
    }

#if 0
    static void rt_alloc(char** ptr, size_t size, void* arg) {
        auto instance = static_cast<RocTracerService*>(arg);

        char* buffer = new char[size];
        instance->m_trace_buffers.push_back(buffer);
        ++instance->m_num_buffers;
        *ptr = buffer;

        if (Log::verbosity() >= 2) {
            unitfmt_result bytes = unitfmt(size, unitfmt_bytes);
            Log(2).stream() << "roctracer: Allocated "
                            << bytes.val << bytes.symbol
                            << " trace buffer" << std::endl;
        }
    }
#endif

    static void rt_activity_callback(const char* begin, const char* end, void* arg) {
        auto instance = static_cast<RocTracerService*>(arg);

        Caliper c;
        instance->flush_activity_records(&c, begin, end);

        if (Log::verbosity() >= 2) {
            unitfmt_result bytes = unitfmt(end-begin, unitfmt_bytes);
            Log(2).stream() << instance->m_channel->name()
                            << ": roctracer: processed "
                            << bytes.val << bytes.symbol
                            << " buffer" << std::endl;
        }
    }

    void init_tracing(Channel* channel) {
        roctracer_properties_t properties {};
        memset(&properties, 0, sizeof(roctracer_properties_t));

        properties.buffer_size = 0x800000;
        // properties.alloc_fun   = rt_alloc;
        // properties.alloc_arg   = this;
        properties.buffer_callback_fun = rt_activity_callback;
        properties.buffer_callback_arg = this;

        if (roctracer_open_pool_expl(&properties, &m_roctracer_pool) != 0) {
            Log(0).stream() << channel->name() << ": roctracer: roctracer_open_pool_expl(): "
                            << roctracer_error_string()
                            << std::endl;
            return;
        }

        if (roctracer_default_pool_expl(m_roctracer_pool) != 0) {
            Log(0).stream() << channel->name() << ": roctracer: roctracer_default_pool_expl(): "
                            << roctracer_error_string()
                            << std::endl;
            return;
        }

        if (roctracer_enable_domain_activity_expl(ACTIVITY_DOMAIN_HIP_OPS, m_roctracer_pool) != 0) {
            Log(0).stream() << channel->name() << ": roctracer: roctracer_enable_activity_expl(): "
                            << roctracer_error_string()
                            << std::endl;
            return;
        }
        if (roctracer_enable_domain_activity_expl(ACTIVITY_DOMAIN_HCC_OPS, m_roctracer_pool) != 0) {
            Log(0).stream() << channel->name() << ": roctracer: roctracer_enable_activity_expl(): "
                            << roctracer_error_string()
                            << std::endl;
            return;
        }

        channel->events().pre_flush_evt.connect(
            [this](Caliper*, Channel*, SnapshotView){
                this->pre_flush_cb();
            });

        Log(1).stream() << channel->name() << ": roctracer: Tracing initialized" << std::endl;
    }

    void init_callbacks(Channel* channel) {
        roctracer_set_properties(ACTIVITY_DOMAIN_HIP_API, NULL);

        if (roctracer_enable_domain_callback(ACTIVITY_DOMAIN_HIP_API, RocTracerService::hip_api_callback, this) != 0) {
            Log(0).stream() << channel->name() << ": roctracer: enable callback (HIP): "
                            << roctracer_error_string()
                            << std::endl;
            return;
        }

        Log(1).stream() << channel->name() << ": roctracer: Callbacks initialized" << std::endl;
    }

    void finish_tracing(Channel* channel) {
        roctracer_disable_domain_activity(ACTIVITY_DOMAIN_HCC_OPS);
        roctracer_disable_domain_activity(ACTIVITY_DOMAIN_HIP_OPS);
        roctracer_close_pool_expl(m_roctracer_pool);
        m_roctracer_pool = nullptr;

        Log(1).stream() << channel->name() << ": roctracer: Tracing stopped" << std::endl;
    }

    void finish_callbacks(Channel* channel) {
        roctracer_disable_domain_callback(ACTIVITY_DOMAIN_HIP_API);

        Log(1).stream() << channel->name() << ": roctracer: Callbacks stopped" << std::endl;
    }

    void post_init_cb(Caliper* c, Channel* channel) {
        subscribe_attributes(c, channel);

        uint64_t starttime = 0;
        roctracer_get_timestamp(&starttime);

        c->set(m_host_starttime_attr, cali_make_variant_from_uint(starttime));

        if (m_record_host_timestamp || m_record_host_duration) {
            c->set(m_host_timestamp_attr, cali_make_variant_from_uint(starttime));

            channel->events().snapshot.connect(
                [](Caliper* c, Channel* chn, int scopes, SnapshotView info, SnapshotBuilder& rec){
                    s_instance->snapshot_cb(c, chn, scopes, info, rec);
                });
        }

        init_callbacks(channel); // apparently must happen before init_tracing()

        if (m_enable_tracing)
            init_tracing(channel);
    }

    void pre_finish_cb(Caliper* c, Channel* channel) {
        finish_callbacks(channel);

        if (m_enable_tracing)
            finish_tracing(channel);
    }

    void finish_cb(Caliper* c, Channel* channel) {
        if (m_enable_tracing) {
            Log(1).stream() << channel->name() << ": roctracer: "
                << m_num_flushes << " activity flushes, "
                << m_num_records << " records processed, "
                << m_num_flushed << " records flushed."
                << std::endl;

            if (Log::verbosity() >= 2) {
                Log(2).stream() << channel->name() << ": roctracer: "
                    << m_num_correlations_stored << " correlations stored; "
                    << m_num_correlations_found  << " correlations found, "
                    << m_num_correlations_missed << " missed."
                    << std::endl;
            }
        }
    }

    RocTracerService(Caliper* c, Channel* channel)
        : m_api_attr       { Attribute::invalid },
          m_num_records    { 0 },
          m_num_flushed    { 0 },
          m_num_flushes    { 0 },
          m_num_correlations_stored { 0 },
          m_num_correlations_found  { 0 },
          m_num_correlations_missed { 0 },
          m_roctracer_pool { nullptr },
          m_channel        { channel }
    {
        auto config = services::init_config_from_spec(channel->config(), s_spec);

        m_enable_tracing = config.get("trace_activities").to_bool();
        m_record_names   = config.get("record_kernel_names").to_bool();
        m_record_host_duration  =
            config.get("snapshot_duration").to_bool();
        m_record_host_timestamp =
            config.get("snapshot_timestamps").to_bool();

        create_callback_attributes(c);
        create_activity_attributes(c);
        create_host_attributes(c);
    }

    ~RocTracerService() {
#if 0
        for (char* buffer : m_trace_buffers)
            delete[] buffer;
#endif
    }

public:

    static const char* s_spec;

    static void register_roctracer(Caliper* c, Channel* channel) {
        if (s_instance) {
            Log(0).stream() << channel->name()
                            << ": roctracer service is already active, disabling!"
                            << std::endl;
        }

        s_instance = new RocTracerService(c, channel);

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
                s_instance->finish_cb(c, channel);
                delete s_instance;
                s_instance = nullptr;
            });

        Log(1).stream() << channel->name()
                        << ": Registered roctracer service."
                        << " Activity tracing is "
                        << (s_instance->m_enable_tracing ? "on" : "off")
                        << std::endl;
    }
};

const char* RocTracerService::s_spec = R"json(
{   "name": "roctracer",
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

RocTracerService* RocTracerService::s_instance = nullptr;

} // namespace [anonymous]

namespace cali
{

CaliperService roctracer_service { ::RocTracerService::s_spec, ::RocTracerService::register_roctracer };

}

// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include "caliper/common/c-util/unitfmt.h"

#include <roctracer.h>
#if 0
#include <roctracer_ext.h>
#include <roctracer_hip.h>
#endif

using namespace cali;

namespace
{

class RocTracerService {
    Attribute m_api_attr;

    Attribute m_activity_start_attr;
    Attribute m_activity_end_attr;
    Attribute m_activity_duration_attr;
    Attribute m_activity_name_attr;
    Attribute m_activity_stream_id_attr;
    Attribute m_activity_device_id_attr;

    unsigned  m_num_buffers;
    unsigned  m_num_flushes;

    std::vector<char*> m_trace_buffers;

    struct buffer_chunk_t {
        const char* begin;
        const char* end;
    };

    std::vector<buffer_chunk_t> m_flushed_chunks;

    roctracer_pool_t* m_roctracer_pool;

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

        Attribute aggr_attr = c->get_attribute("class.aggregatable");
        Variant v_true(true);

        m_activity_duration_attr =
            c->create_attribute("rocm.activity.duration", CALI_TYPE_UINT,
                                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                                1, &aggr_attr, &v_true);

        m_activity_name_attr =
            c->create_attribute("rocm.activity", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
        m_activity_stream_id_attr =
            c->create_attribute("rocm.activity.stream", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
        m_activity_device_id_attr =
            c->create_attribute("rocm.activity.device", CALI_TYPE_UINT, CALI_ATTR_SKIP_EVENTS);
    }

    void subscribe_attributes(Caliper* c, Channel* channel) {
        channel->events().subscribe_attribute(c, channel, m_api_attr);
    }
#if 0
    static void kfd_api_callback(uint32_t domain, uint32_t cid, const void* callback_data, void* arg) {
        auto instance = static_cast<const RocTracerService*>(arg);

        Caliper c;
        auto data = static_cast<const kdf_api_data_t*>(callback_data);
        if (data->phase == ACTIVITY_API_PHASE_ENTER) {
            c.begin(instance->m_api_attr, Variant(roctracer_op_string(ACTIVITY_DOMAIN_KFD_API, cid, 0)));
        } else {
            c.end(instance->m_api_attr);
        }
    }

    static void hip_api_callback(uint32_t domain, uint32_t cid, const void* callback_data, void* arg) {
        auto instance = static_cast<const RocTracerService*>(arg);

        Caliper c;
        auto data = static_cast<const hip_api_data_t*>(callback_data);
        if (data->phase == ACTIVITY_API_PHASE_ENTER) {
            c.begin(instance->m_api_attr, Variant(roctracer_op_string(ACTIVITY_DOMAIN_HIP_API, cid, 0)));
        } else {
            c.end(instance->m_api_attr);
        }
    }

    bool init_callbacks(Channel* channel) {
        int err = roctracer_enable_domain_callback(ACTIVITY_DOMAIN_HIP_API, RocTracerService::hip_api_callback, this);
        if (err != 0)
            Log(0).stream() << channel->name() << ": roctracer: enable callback (HIP): " << roctracer_error_string() << std::endl;

        if (err == 0)
            Log(1).stream() << channel->name() << ": roctracer: enabled callbacks" << std::endl;

        return err == 0;
    }
#endif

    void flush_record(Caliper* c, SnapshotFlushFn snap_fn, const roctracer_record_t* record) {
        Attribute attr[6] = {
            m_activity_device_id_attr,
            // m_activity_stream_id_attr,
            m_activity_name_attr,
            m_activity_start_attr,
            m_activity_end_attr,
            m_activity_duration_attr
        };

        const char* name  = roctracer_op_string(record->domain, record->op, 0);

        Variant data[6] = {
            Variant(cali_make_variant_from_uint(record->device_id)),
            // Variant(cali_make_variant_from_uint(record->stream_id)),
            Variant(name),
            Variant(cali_make_variant_from_uint(record->begin_ns)),
            Variant(cali_make_variant_from_uint(record->end_ns)),
            Variant(cali_make_variant_from_uint(record->end_ns - record->begin_ns))
        };

        SnapshotRecord::FixedSnapshotRecord<8> snapshot_data;
        SnapshotRecord snapshot(snapshot_data);
        c->make_record(5, attr, data, snapshot);
        snap_fn(*c, snapshot.to_entrylist());
    }

    void flush_cb(Caliper* c, Channel* channel, const SnapshotRecord*, SnapshotFlushFn snap_fn) {
        roctracer_flush_activity_expl(m_roctracer_pool);

        unsigned num_records = 0;
        unsigned num_chunks  = 0;

        auto chunks = std::move(m_flushed_chunks);

        for (const buffer_chunk_t& chunk : chunks) {
            const roctracer_record_t* record =
                reinterpret_cast<const roctracer_record_t*>(chunk.begin);
            const roctracer_record_t* end_record =
                reinterpret_cast<const roctracer_record_t*>(chunk.end);

            while (record < end_record) {
                flush_record(c, snap_fn, record);
                ++num_records;
                if (roctracer_next_record(record, &record) != 0)
                    break;
            }

            ++num_chunks;
        }

        Log(1).stream() << channel->name() << ": roctracer: Flushed "
                        << num_records << " records in "
                        << num_chunks  << " chunk(s)"
                        << std::endl;
    }

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

    static void rt_activity_callback(const char* begin, const char* end, void* arg) {
        auto instance = static_cast<RocTracerService*>(arg);

        buffer_chunk_t chunk = { begin, end };
        instance->m_flushed_chunks.push_back(chunk);
        ++instance->m_num_flushes;
    }

    void init_tracing(Channel* channel) {
        roctracer_properties_t properties {};

        properties.buffer_size = 0x1000;
        properties.alloc_fun   = rt_alloc;
        properties.alloc_arg   = this;
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

        if (roctracer_enable_activity_expl(m_roctracer_pool) != 0) {
            Log(0).stream() << channel->name() << ": roctracer: roctracer_enable_activity_expl(): "
                            << roctracer_error_string()
                            << std::endl;
            return;
        }

        channel->events().flush_evt.connect(
            [this](Caliper* c, Channel* chn, const SnapshotRecord* info, SnapshotFlushFn flush_fn){
                this->flush_cb(c, chn, info, flush_fn);
            });

        Log(1).stream() << channel->name() << ": roctracer: Tracing initialized" << std::endl;
    }

    void finish_tracing(Channel* channel) {
        roctracer_disable_activity();
        roctracer_close_pool_expl(m_roctracer_pool);
        m_roctracer_pool = nullptr;

        Log(1).stream() << channel->name() << ": roctracer: Tracing stopped" << std::endl;
    }

    void stop_callbacks(Channel* channel) {
        // roctracer_disable_domain_callback(ACTIVITY_DOMAIN_HIP_API);

        Log(1).stream() << channel->name() << ": roctracer: callbacks disabled" << std::endl;
    }

    void finish_cb(Caliper* c, Channel* channel) {
        // stop_callbacks(channel);
        finish_tracing(channel);

        Log(1).stream() << channel->name() << ": roctracer: "
                        << m_num_buffers << " allocated, "
                        << m_num_flushes << " activity flushes"
                        << std::endl;
    }

    RocTracerService(Caliper* c)
        : m_api_attr       { Attribute::invalid },
          m_num_buffers    { 0 },
          m_num_flushes    { 0 },
          m_roctracer_pool { nullptr }
    {
        create_callback_attributes(c);
        create_activity_attributes(c);
    }

    ~RocTracerService() {
        for (char* buffer : m_trace_buffers)
            delete[] buffer;
    }

public:

    static void register_roctracer(Caliper* c, Channel* channel) {
        RocTracerService* instance = new RocTracerService(c);

        channel->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->subscribe_attributes(c, channel);
                instance->init_tracing(channel);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish_cb(c, channel);
                delete instance;
            });
    }
};

} // namespace [anonymous]

namespace cali
{

CaliperService roctracer_service { "roctracer", ::RocTracerService::register_roctracer };

}
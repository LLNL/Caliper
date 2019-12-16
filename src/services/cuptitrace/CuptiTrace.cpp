// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// CuptiTrace.cpp
// Implementation of the CUpti trace service

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/c-util/unitfmt.h"

#include <cupti.h>

#include <cuda_runtime_api.h>

#include <nvToolsExt.h>
#if CUDART_VERSION >= 9000
#include <nvToolsExtSync.h>
#endif
#include <generated_nvtx_meta.h>

#include <iomanip>
#include <iterator>
#include <mutex>
#include <sstream>
#include <unordered_map>

using namespace cali;

namespace
{

class CuptiTraceService
{
    static const ConfigSet::Entry s_configdata[];

    struct ActivityBuffer {
        uint8_t*        buffer;
        CUcontext       ctx;
        uint32_t        stream_id;
        size_t          size;
        size_t          valid_size;

        ActivityBuffer* next;
        ActivityBuffer* prev;

        ActivityBuffer(CUcontext ctx_, uint32_t stream_, uint8_t* buffer_, size_t size_, size_t valid_size_)
            : buffer(buffer_),
              ctx(ctx_),
              stream_id(stream_),
              size(size_),
              valid_size(valid_size_),
              next(nullptr),
              prev(nullptr)
            { }

        void unlink() {
            if (next)
                next->prev = prev;
            if (prev)
                prev->next = next;
        }
    };

    struct DeviceInfo {
        uint32_t    id;
        const char* name;
        CUuuid      uuid;
        std::string uuid_string;
    };

    std::map<uint32_t, DeviceInfo> device_info_map;

    size_t          buffer_size             = 1 * 1024 * 1024;
    size_t          buffer_size_used        = 0;

    ActivityBuffer* retired_buffers_list    = nullptr;
    std::mutex      retired_buffers_list_lock;

    unsigned        num_buffers_empty       = 0;
    unsigned        num_buffers_allocated   = 0;
    unsigned        num_buffers_completed   = 0;
    unsigned        num_dropped_records     = 0;

    unsigned        num_correlation_recs    = 0;
    unsigned        num_device_recs         = 0;
    unsigned        num_kernel_recs         = 0;
    unsigned        num_driver_recs         = 0;
    unsigned        num_memcpy_recs         = 0;
    unsigned        num_runtime_recs        = 0;
    unsigned        num_unknown_recs        = 0;

    unsigned        num_correlations_found  = 0;
    unsigned        num_correlations_missed = 0;

    Attribute       activity_start_attr;
    Attribute       activity_end_attr;
    Attribute       activity_duration_attr;
    Attribute       activity_kind_attr;
    Attribute       kernel_name_attr;
    Attribute       memcpy_kind_attr;
    Attribute       memcpy_bytes_attr;
    Attribute       starttime_attr;
    Attribute       timestamp_attr;
    Attribute       duration_attr;
    Attribute       device_uuid_attr;

    bool            record_host_timestamp = false;
    bool            record_host_duration  = false;

    static CuptiTraceService* s_instance;

    typedef std::unordered_map<uint32_t, uint64_t> correlation_id_map_t;

    // --- Helpers
    //

    static void
    print_cupti_error(std::ostream& os, CUptiResult err, const char* func) {
        const char* errstr;

        cuptiGetResultString(err, &errstr);

        os << "cupti: " << func << ": error: " << errstr << std::endl;
    }

    // --- CUpti buffer management callbacks
    //

    static void CUPTIAPI buffer_requested(uint8_t** buffer, size_t* size, size_t* max_num_recs) {
        *buffer = new uint8_t[s_instance->buffer_size];

        *size = s_instance->buffer_size;
        *max_num_recs = 0;

        ++s_instance->num_buffers_allocated;
    }

    void add_completed_buffer(ActivityBuffer* acb, size_t dropped) {
        if (!acb->valid_size)
            ++num_buffers_empty;

        num_dropped_records += dropped;

        std::lock_guard<std::mutex>
            g(retired_buffers_list_lock);

        if (retired_buffers_list)
            retired_buffers_list->prev = acb;

        acb->next = retired_buffers_list;
        retired_buffers_list = acb;

        ++num_buffers_completed;
    }

    static void CUPTIAPI buffer_completed(CUcontext ctx, uint32_t stream, uint8_t* buffer, size_t size, size_t valid_size) {
        ActivityBuffer* acb =
            new ActivityBuffer(ctx, stream, buffer, size, valid_size);

        size_t dropped = 0;
        cuptiActivityGetNumDroppedRecords(ctx, stream, &dropped);

        s_instance->add_completed_buffer(acb, dropped);
    }

    // --- Caliper flush
    //

    const char*
    get_memcpy_kind_string(CUpti_ActivityMemcpyKind kind) {
        switch (kind) {
        case CUPTI_ACTIVITY_MEMCPY_KIND_HTOD:
            return "HtoD";
        case CUPTI_ACTIVITY_MEMCPY_KIND_DTOH:
            return "DtoH";
        case CUPTI_ACTIVITY_MEMCPY_KIND_HTOA:
            return "HtoA";
        case CUPTI_ACTIVITY_MEMCPY_KIND_ATOH:
            return "AtoH";
        case CUPTI_ACTIVITY_MEMCPY_KIND_ATOA:
            return "AtoA";
        case CUPTI_ACTIVITY_MEMCPY_KIND_ATOD:
            return "AtoD";
        case CUPTI_ACTIVITY_MEMCPY_KIND_DTOA:
            return "DtoA";
        case CUPTI_ACTIVITY_MEMCPY_KIND_DTOD:
            return "DtoD";
        case CUPTI_ACTIVITY_MEMCPY_KIND_HTOH:
            return "HtoH";
        default:
            break;
        }

        return "<unknown>";
    }

    size_t
    flush_record(CUpti_Activity* rec, correlation_id_map_t& correlation_map, Caliper* c, const SnapshotRecord* flush_info, SnapshotFlushFn proc_fn) {
        switch (rec->kind) {
        case CUPTI_ACTIVITY_KIND_DEVICE:
        {
            CUpti_ActivityDevice2* device =
                reinterpret_cast<CUpti_ActivityDevice2*>(rec);

            DeviceInfo info;

            info.id   = device->id;
            info.name = device->name;
            info.uuid = device->uuid;

            {
                // make a string with the uuid bytes in hex representation

                std::ostringstream os;

                std::copy(device->uuid.bytes, device->uuid.bytes+sizeof(device->uuid.bytes),
                          std::ostream_iterator<unsigned>(os << std::hex << std::setw(2) << std::setfill('0')));

                info.uuid_string = os.str();
            }

            device_info_map[device->id] = info;

            ++num_device_recs;

            return 0;
        }
        case CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION:
        {
            CUpti_ActivityExternalCorrelation* exco =
                reinterpret_cast<CUpti_ActivityExternalCorrelation*>(rec);

            if (exco->externalKind == CUPTI_EXTERNAL_CORRELATION_KIND_CUSTOM0)
                correlation_map[exco->correlationId] = exco->externalId;

            ++num_correlation_recs;

            return 0;
        }
        case CUPTI_ACTIVITY_KIND_DRIVER:
        {
            CUpti_ActivityAPI* api =
                reinterpret_cast<CUpti_ActivityAPI*>(rec);

            ++num_driver_recs;

            return 0;
        }
        case CUPTI_ACTIVITY_KIND_RUNTIME:
        {
            CUpti_ActivityAPI* api =
                reinterpret_cast<CUpti_ActivityAPI*>(rec);

            ++num_runtime_recs;

            return 0;
        }
        case CUPTI_ACTIVITY_KIND_MEMCPY:
        {
            CUpti_ActivityMemcpy* memcpy =
                reinterpret_cast<CUpti_ActivityMemcpy*>(rec);

            Node* parent = nullptr;

            // find a Caliper context correlation, if any

            auto it = correlation_map.find(memcpy->correlationId);

            if (it != correlation_map.end()) {
                parent = c->node(it->second);

                ++num_correlations_found;
                correlation_map.erase(it);
            } else {
                ++num_correlations_missed;
            }

            // find a device info record

            {
                auto it = device_info_map.find(memcpy->deviceId);

                if (it != device_info_map.end())
                    parent =
                        c->make_tree_entry(device_uuid_attr,
                                           Variant(CALI_TYPE_STRING,
                                                   it->second.uuid_string.c_str(),
                                                   it->second.uuid_string.size() + 1),
                                           parent);
            }

            // append the memcpy info

            Attribute attr[6] = {
                activity_kind_attr,
                memcpy_kind_attr,
                memcpy_bytes_attr,
                activity_start_attr,
                activity_end_attr,
                activity_duration_attr
            };
            Variant   data[6] = {
                Variant(CALI_TYPE_STRING, "memcpy", 7),
                Variant(CALI_TYPE_STRING,
                        get_memcpy_kind_string(static_cast<CUpti_ActivityMemcpyKind>(memcpy->copyKind)),
                        5),
                Variant(cali_make_variant_from_uint(memcpy->bytes)),
                Variant(cali_make_variant_from_uint(memcpy->start)),
                Variant(cali_make_variant_from_uint(memcpy->end)),
                Variant(cali_make_variant_from_uint(memcpy->end - memcpy->start))
            };

            SnapshotRecord::FixedSnapshotRecord<8> snapshot_data;
            SnapshotRecord snapshot(snapshot_data);

            c->make_record(6, attr, data, snapshot, parent);

            proc_fn(*c, snapshot.to_entrylist());

            ++num_memcpy_recs;

            return 1;
        }
        case CUPTI_ACTIVITY_KIND_KERNEL:
        case CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL:
        {
            CUpti_ActivityKernel4* kernel =
                reinterpret_cast<CUpti_ActivityKernel4*>(rec);

            Node* parent = nullptr;

            // find a Caliper context correlation, if any

            {
                auto it = correlation_map.find(kernel->correlationId);

                if (it != correlation_map.end()) {
                    parent = c->node(it->second);

                    ++num_correlations_found;
                    correlation_map.erase(it);
                } else {
                    ++num_correlations_missed;
                }
            }

            // find a device info record

            {
                auto it = device_info_map.find(kernel->deviceId);

                if (it != device_info_map.end())
                    parent =
                        c->make_tree_entry(device_uuid_attr,
                                           Variant(CALI_TYPE_STRING,
                                                   it->second.uuid_string.c_str(),
                                                   it->second.uuid_string.size() + 1),
                                           parent);
            }

            // append the kernel info

            Attribute attr[5] = {
                activity_kind_attr,
                kernel_name_attr,
                activity_start_attr,
                activity_end_attr,
                activity_duration_attr
            };
            Variant   data[5] = {
                Variant(CALI_TYPE_STRING, "kernel", 7),
                Variant(CALI_TYPE_STRING, kernel->name, strlen(kernel->name)+1),
                Variant(cali_make_variant_from_uint(kernel->start)),
                Variant(cali_make_variant_from_uint(kernel->end)),
                Variant(cali_make_variant_from_uint(kernel->end - kernel->start))
            };

            SnapshotRecord::FixedSnapshotRecord<8> snapshot_data;
            SnapshotRecord snapshot(snapshot_data);

            c->make_record(5, attr, data, snapshot, parent);

            proc_fn(*c, snapshot.to_entrylist());

            ++num_kernel_recs;

            return 1;
        }
        default:
            ++num_unknown_recs;
        }

        return 0;
    }

    size_t
    flush_buffer(ActivityBuffer* acb, Caliper* c, const SnapshotRecord* flush_info, SnapshotFlushFn proc_fn) {
        if (! (acb->valid_size > 0))
            return 0;

        size_t num_records  = 0;

        CUpti_Activity* rec = nullptr;
        CUptiResult     res = CUPTI_SUCCESS;

        correlation_id_map_t correlation_map(2000);

        do {
            res = cuptiActivityGetNextRecord(acb->buffer, acb->valid_size, &rec);

            if (res == CUPTI_SUCCESS)
                num_records += flush_record(rec, correlation_map, c, flush_info, proc_fn);
        } while (res == CUPTI_SUCCESS);

        if (res != CUPTI_SUCCESS && res != CUPTI_ERROR_MAX_LIMIT_REACHED)
            print_cupti_error(Log(0).stream(), res, "cuptiActivityGetNextRecord");

        return num_records;
    }

    // --- Caliper callbacks
    //

    void flush_cb(Caliper* c, Channel* chn, const SnapshotRecord* flush_info, SnapshotFlushFn proc_fn) {
        //   Flush CUpti. Apppends all currently active CUpti trace buffers
        // to the retired_buffers_list.

        CUptiResult res = cuptiActivityFlushAll(CUPTI_ACTIVITY_FLAG_NONE);

        if (res != CUPTI_SUCCESS) {
            print_cupti_error(Log(0).stream(), res, "cuptiActivityFlushAll");
            return;
        }

        // go through all stored buffers and flush them

        ActivityBuffer* acb = nullptr;

        {
            std::lock_guard<std::mutex>
                g(retired_buffers_list_lock);

            acb = retired_buffers_list;
        }

        size_t num_written = 0;

        for ( ; acb; acb = acb->next )
            num_written += flush_buffer(acb, c, flush_info, proc_fn);

        Log(1).stream() << "cuptitrace: Wrote " << num_written << " records." << std::endl;
    }

    void clear_cb(Caliper* c, Channel* chn) {
        ActivityBuffer* acb = nullptr;

        {
            std::lock_guard<std::mutex>
                g(retired_buffers_list_lock);

            acb = retired_buffers_list;
            retired_buffers_list = nullptr;
        }

        while (acb) {
            ActivityBuffer* tmp = acb->next;

            buffer_size_used += acb->valid_size;

            acb->unlink();

            delete[] acb->buffer;
            delete   acb;

            acb = tmp;
        }
    }

    void post_begin_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) {
        if (attr.is_nested()) {
            Entry e = c->get(attr);

            if (e.is_reference()) {
                CUptiResult res =
                    cuptiActivityPushExternalCorrelationId(CUPTI_EXTERNAL_CORRELATION_KIND_CUSTOM0, e.node()->id());

                if (res != CUPTI_SUCCESS)
                    print_cupti_error(Log(0).stream(), res, "cuptiActivityPushExternalCorrelationId");
            }
        }
    }

    void pre_end_cb(Caliper* c, Channel* chn, const Attribute& attr, const Variant& value) {
        if (attr.is_nested()) {
            CUptiResult res =
                cuptiActivityPopExternalCorrelationId(CUPTI_EXTERNAL_CORRELATION_KIND_CUSTOM0, nullptr);

            if (res != CUPTI_SUCCESS)
                print_cupti_error(Log(0).stream(), res, "cuptiActivityPopExternalCorrelationId");
        }
    }

    void finish_cb(Caliper* c, Channel* chn) {
        cuptiFinalize();

        if (Log::verbosity() < 1)
            return;

        if (num_dropped_records > 0)
            Log(1).stream() << chn->name() << ": cuptitrace: Dropped " << num_dropped_records
                            << " records." << std::endl;

        unitfmt_result bytes_reserved =
            unitfmt(num_buffers_completed * buffer_size, unitfmt_bytes);
        unitfmt_result bytes_used =
            unitfmt(buffer_size_used, unitfmt_bytes);

        Log(1).stream() << chn->name() << ": cuptitrace: Allocated " << num_buffers_allocated
                        << " buffers ("
                        << bytes_reserved.val << bytes_reserved.symbol
                        << " reserved, "
                        << bytes_used.val     << bytes_used.symbol
                        << " used). "
                        << num_buffers_completed << " buffers completed, "
                        << num_buffers_empty << " empty." << std::endl;

        if (Log::verbosity() < 2)
            return;

        Log(2).stream() << chn->name() << ": cuptitrace: Processed CUpti activity records:"
                        << "\n  correlation records: " << num_correlation_recs
                        << "\n  device records:      " << num_device_recs
                        << "\n  driver records:      " << num_driver_recs
                        << "\n  runtime records:     " << num_runtime_recs
                        << "\n  kernel records:      " << num_kernel_recs
                        << "\n  memcpy records:      " << num_memcpy_recs
                        << "\n  unknown records:     " << num_unknown_recs
                        << std::endl;

        Log(2).stream() << chn->name() << ": cuptitrace: "
                        << num_correlations_found  << " context correlations found, "
                        << num_correlations_missed << " missed."
                        << std::endl;
    }

    void enable_cupti_activities(const ConfigSet& config) {
        struct activity_map_t {
            const char*        name;
            CUpti_ActivityKind kind;
        } activity_map[] = {
            { "correlation", CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION },
            { "device",      CUPTI_ACTIVITY_KIND_DEVICE  },
            { "driver",      CUPTI_ACTIVITY_KIND_DRIVER  },
            { "runtime",     CUPTI_ACTIVITY_KIND_RUNTIME },
            { "kernel",      CUPTI_ACTIVITY_KIND_KERNEL  },
            { "memcpy",      CUPTI_ACTIVITY_KIND_MEMCPY  },

            { nullptr,       CUPTI_ACTIVITY_KIND_INVALID }
        };

        std::vector<std::string> selection =
            config.get("activities").to_stringlist();

        for (const activity_map_t* act = activity_map; act && act->name; ++act) {
            auto it = std::find(selection.begin(), selection.end(),
                                act->name);

            if (it == selection.end())
                continue;

            selection.erase(it);

            CUptiResult res = cuptiActivityEnable(act->kind);

            if (res != CUPTI_SUCCESS) {
                const char* errstr;
                cuptiGetResultString(res, &errstr);

                Log(0).stream() << "cuptitrace: cuptiActivityEnable ("
                                << act->name << "): " << errstr
                                << std::endl;
            }
        }

        for (const std::string& s : selection)
            Log(0).stream() << "cuptitrace: selected activity \"" << s << "\" not found!" << std::endl;

    }

    void snapshot_cb(Caliper* c, Channel* chn, int scopes, const SnapshotRecord* trigger_info, SnapshotRecord* snapshot) {
        uint64_t timestamp = 0;
        cuptiGetTimestamp(&timestamp);

        Variant  v_now(cali_make_variant_from_uint(timestamp));
        Variant  v_prev = c->exchange(timestamp_attr, v_now);

        if (record_host_duration)
            snapshot->append(duration_attr.id(),
                             Variant(cali_make_variant_from_uint(timestamp - v_prev.to_uint())));
    }

    void post_init_cb(Caliper* c, Channel* chn) {
        ConfigSet config = chn->config().init("cuptitrace", s_configdata);

        enable_cupti_activities(config);

        CUptiResult res =
            cuptiActivityRegisterCallbacks(buffer_requested, buffer_completed);

        if (res != CUPTI_SUCCESS) {
            print_cupti_error(Log(0).stream(), res, "cuptiActivityRegisterCallbacks");
            return;
        }

        uint64_t starttime = 0;
        cuptiGetTimestamp(&starttime);

        c->set(starttime_attr, cali_make_variant_from_uint(starttime));

        if (config.get("correlate_context").to_bool()) {
            chn->events().post_begin_evt.connect(
                [](Caliper* c, Channel* chn, const Attribute& attr, const Variant& value){
                    s_instance->post_begin_cb(c, chn, attr, value);
                });
            chn->events().pre_end_evt.connect(
                [](Caliper* c, Channel* chn, const Attribute& attr, const Variant& value){
                    s_instance->pre_end_cb(c, chn, attr, value);
                });
        }

        if (record_host_timestamp || record_host_duration) {
            c->set(timestamp_attr, cali_make_variant_from_uint(starttime));

            chn->events().snapshot.connect(
                [](Caliper* c, Channel* chn, int scopes, const SnapshotRecord* info, SnapshotRecord* rec){
                    s_instance->snapshot_cb(c, chn, scopes, info, rec);
                });
        }

        chn->events().flush_evt.connect(
            [](Caliper* c, Channel* chn, const SnapshotRecord* info, SnapshotFlushFn flush_fn){
                s_instance->flush_cb(c, chn, info, flush_fn);
            });
        chn->events().clear_evt.connect(
            [](Caliper* c, Channel* chn){
                s_instance->clear_cb(c, chn);
            });
        chn->events().finish_evt.connect(
            [](Caliper* c, Channel* chn){
                s_instance->finish_cb(c, chn);
                delete s_instance;
                s_instance = nullptr;
            });

        Log(1).stream() << chn->name() << ": Registered cuptitrace service" << std::endl;
    }

    CuptiTraceService(Caliper* c, Channel* chn)
        {
            Attribute unit_attr =
                c->create_attribute("time.unit", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);
            Attribute aggr_class_attr =
                c->get_attribute("class.aggregatable");
            
            Variant   nsec_val  = Variant(CALI_TYPE_STRING, "nsec", 4);
            Variant   true_val  = Variant(true);

            Attribute meta_attr[2] = { aggr_class_attr, unit_attr };
            Variant   meta_vals[2] = { true_val,        nsec_val  };

            activity_start_attr =
                c->create_attribute("cupti.activity.start",    CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
            activity_end_attr =
                c->create_attribute("cupti.activity.end",      CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
            activity_duration_attr =
                c->create_attribute("cupti.activity.duration", CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                                    2, meta_attr, meta_vals);
            activity_kind_attr =
                c->create_attribute("cupti.activity.kind",     CALI_TYPE_STRING,
                                    CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);
            kernel_name_attr =
                c->create_attribute("cupti.kernel.name",       CALI_TYPE_STRING,
                                    CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);
            memcpy_kind_attr =
                c->create_attribute("cupti.memcpy.kind",       CALI_TYPE_STRING,
                                    CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);
            memcpy_bytes_attr =
                c->create_attribute("cupti.memcpy.bytes",      CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS,
                                    1, &aggr_class_attr, &true_val);
            starttime_attr =
                c->create_attribute("cupti.starttime",         CALI_TYPE_UINT,
                                    CALI_ATTR_SCOPE_PROCESS | 
                                    CALI_ATTR_SKIP_EVENTS);
            device_uuid_attr =
                c->create_attribute("cupti.device.uuid",       CALI_TYPE_STRING,
                                    CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);

            ConfigSet config = chn->config().init("cuptitrace", s_configdata);

            record_host_timestamp = config.get("snapshot_timestamps").to_bool();
            record_host_duration  = config.get("snapshot_duration").to_bool();

            if (record_host_duration || record_host_timestamp) {
                int hide_offset =
                    record_host_timestamp ? 0 : CALI_ATTR_HIDDEN;

                timestamp_attr =
                    c->create_attribute("cupti.timestamp", CALI_TYPE_UINT,
                                        CALI_ATTR_SCOPE_THREAD |
                                        CALI_ATTR_ASVALUE      |
                                        CALI_ATTR_SKIP_EVENTS  |
                                        hide_offset);
                duration_attr =
                    c->create_attribute("cupti.host.duration", CALI_TYPE_UINT,
                                        CALI_ATTR_SCOPE_THREAD |
                                        CALI_ATTR_ASVALUE      |
                                        CALI_ATTR_SKIP_EVENTS,
                                        2, meta_attr, meta_vals);
            }
        }

public:

    static void cuptitrace_initialize(Caliper* c, Channel* chn) {
        if (s_instance) {
            Log(0).stream() << chn->name() << ": cuptitrace service is already initialized!"
                            << std::endl;
            return;
        }

        s_instance = new CuptiTraceService(c, chn);

        chn->events().post_init_evt.connect(
            [](Caliper* c, Channel* chn){
                s_instance->post_init_cb(c, chn);
            });
    }

};

const struct ConfigSet::Entry CuptiTraceService::s_configdata[] = {
    { "activities", CALI_TYPE_STRING, "correlation,device,runtime,kernel,memcpy",
      "The CUpti activity kinds to record",
      "The CUpti activity kinds to record. Possible values: "
      "  device:       Device info"
      "  correlation:  Correlation records. Required for Caliper context correlation."
      "  driver:       Driver API."
      "  runtime:      Runtime API."
      "    Runtime records are also required for Caliper context correlation."
      "  kernel:       CUDA Kernels being executed."
      "  memcpy:       CUDA memory copies."
    },
    { "correlate_context",   CALI_TYPE_BOOL, "true",
      "Correlate CUpti records with Caliper context",
      "Correlate CUpti records with Caliper context" },
    { "snapshot_timestamps", CALI_TYPE_BOOL, "false",
      "Record CUpti timestamps for all Caliper snapshots",
      "Record CUpti timestamps for all Caliper snapshots"
    },
    { "snapshot_duration",   CALI_TYPE_BOOL, "false",
      "Record duration of host-side activities using CUpti timestamps",
      "Record duration of host-side activities using CUpti timestamps"
    },

    ConfigSet::Terminator
};

CuptiTraceService* CuptiTraceService::s_instance = nullptr;

} // namespace [anonymous]

namespace cali
{

CaliperService cuptitrace_service = { "cuptitrace", ::CuptiTraceService::cuptitrace_initialize };

}

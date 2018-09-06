// Copyright (c) 2018, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

#include <mutex>
#include <unordered_map>

using namespace cali;

namespace
{

const ConfigSet::Entry configdata[] = {
    { "activities", CALI_TYPE_STRING, "correlation,runtime,kernel",
      "The CUpti activity kinds to record",
      "The CUpti activity kinds to record. Possible values: "
      "  correlation:  Correlation records. Required for Caliper context correlation."
      "  driver:       Driver API."
      "  runtime:      Runtime API."
      "    Runtime records are also required for Caliper context correlation."
      "  kernel:       CUDA Kernels being executed."
    },
    { "correlate_context", CALI_TYPE_BOOL, "true",
      "Correlate CUpti records with Caliper context",
      "Correlate CUpti records with Caliper context" },

    ConfigSet::Terminator
};

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

size_t          buffer_size = 1 * 1024 * 1024;

size_t          buffer_size_used = 0;

ActivityBuffer* retired_buffers_list = nullptr;
std::mutex      retired_buffers_list_lock;

unsigned        num_buffers_empty       = 0;
unsigned        num_buffers_allocated   = 0;
unsigned        num_buffers_completed   = 0;
unsigned        num_dropped_records     = 0;

unsigned        num_correlation_recs    = 0;
unsigned        num_kernel_recs         = 0;
unsigned        num_driver_recs         = 0;
unsigned        num_runtime_recs        = 0;
unsigned        num_unknown_recs        = 0;

unsigned        num_correlations_found  = 0;
unsigned        num_correlations_missed = 0;

Attribute       activity_start_attr;
Attribute       activity_end_attr;
Attribute       activity_duration_attr;
Attribute       activity_kind_attr;
Attribute       kernel_name_attr;
Attribute       starttime_attr;

typedef std::unordered_map<uint32_t, uint64_t> correlation_id_map_t;

// --- Helpers
//

void
print_cupti_error(std::ostream& os, CUptiResult err, const char* func)
{
    const char* errstr;
    
    cuptiGetResultString(err, &errstr);

    os << "cupti: " << func << ": error: " << errstr << std::endl;
}

// --- CUpti buffer management callbacks
//

void CUPTIAPI buffer_requested(uint8_t** buffer, size_t* size, size_t* max_num_recs)
{
    *buffer = new uint8_t[buffer_size];

    *size = buffer_size;
    *max_num_recs = 0;

    ++num_buffers_allocated;
}

void CUPTIAPI buffer_completed(CUcontext ctx, uint32_t stream, uint8_t* buffer, size_t size, size_t valid_size)
{
    if (!valid_size)
        ++num_buffers_empty;
    
    ActivityBuffer* acb =
        new ActivityBuffer(ctx, stream, buffer, size, valid_size);

    size_t dropped;
    cuptiActivityGetNumDroppedRecords(ctx, stream, &dropped);

    num_dropped_records += dropped;

    std::lock_guard<std::mutex>
        g(retired_buffers_list_lock);

    if (retired_buffers_list)
        retired_buffers_list->prev = acb;

    acb->next = retired_buffers_list;
    retired_buffers_list = acb;

    ++num_buffers_completed;
}

// --- Caliper flush
//

size_t
flush_record(CUpti_Activity* rec, correlation_id_map_t& correlation_map, Caliper* c, const SnapshotRecord* flush_info, Caliper::SnapshotFlushFn proc_fn)
{    
    switch (rec->kind) {
    case CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION:
    {
        CUpti_ActivityExternalCorrelation* correlation =
            reinterpret_cast<CUpti_ActivityExternalCorrelation*>(rec);

        if (correlation->externalKind == CUPTI_EXTERNAL_CORRELATION_KIND_CUSTOM0)
            correlation_map[correlation->correlationId] = correlation->externalId;

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
    case CUPTI_ACTIVITY_KIND_KERNEL:
    case CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL:
    {
        CUpti_ActivityKernel4* kernel =
            reinterpret_cast<CUpti_ActivityKernel4*>(rec);

        Attribute attr[4] = {
            kernel_name_attr,
            activity_start_attr,
            activity_end_attr,
            activity_duration_attr
        };
        Variant   data[4] = {
            Variant(CALI_TYPE_STRING, kernel->name, strlen(kernel->name)+1),
            Variant(cali_make_variant_from_uint(kernel->start)),
            Variant(cali_make_variant_from_uint(kernel->end)),
            Variant(cali_make_variant_from_uint(kernel->end - kernel->start))
        };

        SnapshotRecord::FixedSnapshotRecord<8> snapshot_data;
        SnapshotRecord snapshot(snapshot_data);
        
        c->make_entrylist(4, attr, data, snapshot);

        // find a Caliper context correlation, if any 
        auto it = correlation_map.find(kernel->correlationId);

        if (it != correlation_map.end()) {
            Node* node = c->node(it->second);

            if (node)
                snapshot.append(node);
            
            ++num_correlations_found;
            correlation_map.erase(it);
        } else {
            ++num_correlations_missed;
        }
            
        proc_fn(&snapshot);

        ++num_kernel_recs;

        return 1;
    }
    default:
        ++num_unknown_recs;
    }

    return 0;
}

size_t
flush_buffer(ActivityBuffer* acb, Caliper* c, const SnapshotRecord* flush_info, Caliper::SnapshotFlushFn proc_fn)
{
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

void flush_cb(Caliper* c, const SnapshotRecord* flush_info, Caliper::SnapshotFlushFn proc_fn)
{
    // flush currently active CUpti buffers

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

void clear_cb(Caliper* c)
{
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

void post_begin_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
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

void pre_end_cb(Caliper* c, const Attribute& attr, const Variant& value)
{
    if (attr.is_nested()) {
        CUptiResult res =
            cuptiActivityPopExternalCorrelationId(CUPTI_EXTERNAL_CORRELATION_KIND_CUSTOM0, nullptr);

        if (res != CUPTI_SUCCESS)
            print_cupti_error(Log(0).stream(), res, "cuptiActivityPopExternalCorrelationId");                
    }
}

void finish_cb(Caliper* c)
{
    cuptiFinalize();

    if (Log::verbosity() < 1)
        return;
    
    if (num_dropped_records > 0)
        Log(1).stream() << "cuptitrace: Dropped " << num_dropped_records
                        << " records." << std::endl;

    unitfmt_result bytes_reserved =
        unitfmt(num_buffers_completed * buffer_size, unitfmt_bytes);
    unitfmt_result bytes_used =
        unitfmt(buffer_size_used, unitfmt_bytes);
            
    Log(1).stream() << "cuptitrace: Allocated " << num_buffers_allocated
                    << " buffers ("
                    << bytes_reserved.val << bytes_reserved.symbol
                    << " reserved, "
                    << bytes_used.val     << bytes_used.symbol
                    << " used). "
                    << num_buffers_completed << " buffers completed, "
                    << num_buffers_empty << " empty." << std::endl;

    if (Log::verbosity() >= 2) {
        Log(2).stream() << "cuptitrace: Processed CUpti activity records:"
                        << "\n  correlation records: " << num_correlation_recs
                        << "\n  driver records:      " << num_driver_recs
                        << "\n  runtime records:     " << num_runtime_recs            
                        << "\n  kernel records:      " << num_kernel_recs
                        << "\n  unknown records:     " << num_unknown_recs
                        << std::endl;

        Log(2).stream() << "cuptitrace: "
                        << num_correlations_found  << " context correlations found, "
                        << num_correlations_missed << " missed."
                        << std::endl;
    }
}

void enable_cupti_activities(const ConfigSet& config)
{
    struct activity_map_t {
        const char*        name;
        CUpti_ActivityKind kind;
    } activity_map[] = {
        { "correlation", CUPTI_ACTIVITY_KIND_EXTERNAL_CORRELATION },
        { "driver",      CUPTI_ACTIVITY_KIND_DRIVER  },
        { "runtime",     CUPTI_ACTIVITY_KIND_RUNTIME },
        { "kernel",      CUPTI_ACTIVITY_KIND_KERNEL  },

        { nullptr, CUPTI_ACTIVITY_KIND_INVALID }
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
            
            Log(0).stream() << "cupti: cuptiActivityEnable ("
                            << act->name << "): " << errstr
                            << std::endl;
        }
    }

    for (const std::string& s : selection)
        Log(0).stream() << "cuptitrace: selected activity \"" << s << "\" not found!" << std::endl;
        
}

void post_init_cb(Caliper* c)
{   
    ConfigSet config = RuntimeConfig::init("cuptitrace", configdata);

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
        c->events().post_begin_evt.connect(&post_begin_cb);
        c->events().pre_end_evt.connect(&pre_end_cb);
    }
    
    c->events().flush_evt.connect(&flush_cb);
    c->events().clear_evt.connect(&clear_cb);
    c->events().finish_evt.connect(&finish_cb);

    Log(1).stream() << "Registered cuptitrace service" << std::endl;
}

void cuptitrace_initialize(Caliper* c)
{        
    Attribute aggr_attr = c->get_attribute("class.aggregatable");
    Variant   v_true(true);
    
    activity_start_attr =
        c->create_attribute("cupti.activity.start",    CALI_TYPE_UINT,
                            CALI_ATTR_ASVALUE);
    activity_end_attr =
        c->create_attribute("cupti.activity.end",      CALI_TYPE_UINT,
                            CALI_ATTR_ASVALUE);
    activity_duration_attr =
        c->create_attribute("cupti.activity.duration", CALI_TYPE_UINT,
                            CALI_ATTR_ASVALUE,
                            1, &aggr_attr, &v_true);
    activity_kind_attr =
        c->create_attribute("cupti.activity.kind",     CALI_TYPE_STRING,
                            CALI_ATTR_DEFAULT);
    kernel_name_attr =
        c->create_attribute("cupti.kernel.name",       CALI_TYPE_STRING,
                            CALI_ATTR_DEFAULT);
    starttime_attr =
        c->create_attribute("cupti.starttime",         CALI_TYPE_UINT,
                            CALI_ATTR_SKIP_EVENTS);
    
    c->events().post_init_evt.connect(&post_init_cb);
}

} // namespace [anonymous]

namespace cali
{

CaliperService cuptitrace_service = { "cuptitrace", ::cuptitrace_initialize };

}

// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Cupti.cpp
// Implementation of Cupti service

#include "CuptiEventSampling.h"
#include "../kokkos/KokkosProfilingSymbols.hpp"
#include "caliper/Annotation.h"
#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <cupti.h>

#include <cuda_runtime_api.h>

#include <nvToolsExt.h>
#if CUDART_VERSION >= 9000
#include <nvToolsExtSync.h>
#endif
#include <generated_nvtx_meta.h>

#define CUPTI_CALL(call)                                                    \
do {                                                                        \
    CUptiResult _status = call;                                             \
    if (_status != CUPTI_SUCCESS) {                                         \
      const char *errstr;                                                   \
      cuptiGetResultString(_status, &errstr);                               \
      fprintf(stderr, "%s:%d: error: function %s failed with error %s.\n",  \
              __FILE__, __LINE__, #call, errstr);                           \
      exit(-1);                                                             \
    }                                                                       \
} while (0)

#include <vector>
#define DRIVER_API_CALL(apiFuncCall)                                           \
do {                                                                           \
    CUresult _status = apiFuncCall;                                            \
    if (_status != CUDA_SUCCESS) {                                             \
        fprintf(stderr, "%s:%d: error: function %s failed with error %d.\n",   \
                __FILE__, __LINE__, #apiFuncCall, _status);                    \
        exit(-1);                                                              \
    }                                                                          \
} while (0)

#define RUNTIME_API_CALL(apiFuncCall)                                          \
do {                                                                           \
    cudaError_t _status = apiFuncCall;                                         \
    if (_status != cudaSuccess) {                                              \
        fprintf(stderr, "%s:%d: error: function %s failed with error %s.\n",   \
                __FILE__, __LINE__, #apiFuncCall, cudaGetErrorString(_status));\
        exit(-1);                                                              \
    }                                                                          \
} while (0)

#define BUF_SIZE (8 * 1024)
#define ALIGN_SIZE (8)
#define ALIGN_BUFFER(buffer, align)                                            \
    (((uintptr_t) (buffer) & ((align)-1)) ? ((buffer) + (align) - ((uintptr_t) (buffer) & ((align)-1))) : (buffer))

using namespace cali;
cali::Attribute fault_address_attr;
cali::Attribute direction_attr;
cali::Attribute bytes_attr;
Channel* global_chn;
static const char *
getUvmCounterKindString(CUpti_ActivityUnifiedMemoryCounterKind kind)
{
    switch (kind)
    {
    case CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_BYTES_TRANSFER_HTOD:
        return "BYTES_TRANSFER_HTOD";
    case CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_BYTES_TRANSFER_DTOH:
        return "BYTES_TRANSFER_DTOH";
    default:
        break;
    }
    return "<unknown>";
}

static void
printActivity(CUpti_Activity *record)
{
    Caliper c;
    switch (record->kind)
    {
    case CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER:
        {
            if(!global_chn){ std::cout <<"No global channel\n";return; }
    std::cout<<"Flushing CUPTI activity\n";
            CUpti_ActivityUnifiedMemoryCounter2 *uvm = (CUpti_ActivityUnifiedMemoryCounter2 *)record;

            cali_id_t attr[3] = { fault_address_attr.id(), direction_attr.id(), bytes_attr.id() };
            Attribute raw_attr[3] = { fault_address_attr, direction_attr, bytes_attr };
            Variant data[3] = {
              Variant(CALI_TYPE_ADDR, &uvm->address, sizeof(void*)),
              Variant(getUvmCounterKindString(uvm->counterKind)),
              Variant(cali_make_variant_from_uint(uvm->value)) 
            };
            SnapshotRecord::FixedSnapshotRecord<3> info_data;
            SnapshotRecord info(info_data);
            c.make_record(3, raw_attr, data, info);
            //c.push_snapshot(global_chn, &info);
            //SnapshotRecord current(0, nullptr, 80, nullptr, nullptr);
            //c.pull_snapshot(global_chn, CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, nullptr, &current);
            //info.append(current);
            c.push_snapshot(global_chn, &info);
              
            //cali::Annotation aa("cupti.uvm.address");
            //cali::Annotation ba("cupti.uvm.direction");
            //cali::Annotation ca("cupti.uvm.bytes");
            //long unsigned int address = uvm->address;
            //long unsigned int value = uvm->value;
            //aa.begin(Variant(CALI_TYPE_ADDR,&address,sizeof(void*)));
            //ba.begin(getUvmCounterKindString(uvm->counterKind));
            //ca.begin(value);
            //ca.end();
            //ba.end();
            //aa.end();
            


            //
            //printf("UNIFIED_MEMORY_COUNTER %p [ %llu %llu ] kind=%s value=%llu src %u dst %u\n",
            //    (uvm->address),
            //    (unsigned long long)(uvm->start),
            //    (unsigned long long)(uvm->end),
            //    getUvmCounterKindString(uvm->counterKind),
            //    (unsigned long long)uvm->value,
            //    uvm->srcId,
            //    uvm->dstId);
            break;
        }
    }
}

static void CUPTIAPI
bufferRequested(uint8_t **buffer, size_t *size, size_t *maxNumRecords)
{
    uint8_t *rawBuffer;

    *size = BUF_SIZE;
    rawBuffer = (uint8_t *)malloc(*size + ALIGN_SIZE);

    *buffer = ALIGN_BUFFER(rawBuffer, ALIGN_SIZE);
    *maxNumRecords = 0;

    if (*buffer == NULL) {
        printf("Error: out of memory\n");
        exit(-1);
    }
}

static void CUPTIAPI
bufferCompleted(CUcontext ctx, uint32_t streamId, uint8_t *buffer, size_t size, size_t validSize)
{
    CUptiResult status;
    CUpti_Activity *record = NULL;

    do {
        status = cuptiActivityGetNextRecord(buffer, validSize, &record);
        if (status == CUPTI_SUCCESS) {
            printActivity(record);
        }
        else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED) {
            break;
        }
        else {
            CUPTI_CALL(status);
        }
    } while (1);
    // report any records dropped from the queue
    size_t dropped;
    CUPTI_CALL(cuptiActivityGetNumDroppedRecords(ctx, streamId, &dropped));
    if (dropped != 0) {
        printf("Dropped %u activity records\n", (unsigned int)dropped);
    }

    free(buffer);
}

static void initialize_uvm_callbacks(Caliper* c, Channel* chn){
 CUptiResult res;
    global_chn = chn; // TODO DZP: no
    int *data = NULL;
    int size = 64*1024;     // 64 KB
    int i = 123;
    CUpti_ActivityUnifiedMemoryCounterConfig config[2];
    Attribute class_mem = c->get_attribute("class.memoryaddress");
    Variant   v_true(true);

    fault_address_attr = c->create_attribute("cupti.uvm.address", CALI_TYPE_ADDR,
        CALI_ATTR_ASVALUE ,
        1, &class_mem, &v_true);
    direction_attr = c->create_attribute("cupti.uvm.direction", CALI_TYPE_STRING, CALI_ATTR_ASVALUE );
    bytes_attr = c->create_attribute("cupti.uvm.bytes", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);
    

    //DRIVER_API_CALL(cuInit(0));

    //DRIVER_API_CALL(cuDeviceGetCount(&deviceCount));

    // register cupti activity buffer callbacks
    CUPTI_CALL(cuptiActivityRegisterCallbacks(bufferRequested, bufferCompleted));
   // configure unified memory counters
    config[0].scope = CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_SCOPE_PROCESS_SINGLE_DEVICE;
    config[0].kind = CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_BYTES_TRANSFER_HTOD;
    config[0].deviceId = 0;
    config[0].enable = 1;

    config[1].scope = CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_SCOPE_PROCESS_SINGLE_DEVICE;
    config[1].kind = CUPTI_ACTIVITY_UNIFIED_MEMORY_COUNTER_KIND_BYTES_TRANSFER_DTOH;
    config[1].deviceId = 0;
    config[1].enable = 1;

    res = cuptiActivityConfigureUnifiedMemoryCounter(config, 2);
    if (res == CUPTI_ERROR_UM_PROFILING_NOT_SUPPORTED) {
        printf("Test is waived, unified memory is not supported on the underlying platform.\n");
        return;
    }
    else if (res == CUPTI_ERROR_UM_PROFILING_NOT_SUPPORTED_ON_DEVICE) {
        printf("Test is waived, unified memory is not supported on the device.\n");
        return;
    }
    else if (res == CUPTI_ERROR_UM_PROFILING_NOT_SUPPORTED_ON_NON_P2P_DEVICES) {
        printf("Test is waived, unified memory is not supported on the non-P2P multi-gpu setup.\n");
        return;
    }
 else {
        CUPTI_CALL(res);
    }
    std::cout <<"Registering UVM service\n";
    // enable unified memory counter activity
    CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER));
kokkosp_callbacks.kokkosp_end_parallel_for_callback.connect([&](const uint64_t){
        CUPTI_CALL(cuptiActivityFlushAll(0));
});
    chn->events().finish_evt.connect([](Caliper* c, Channel* chn){
        CUPTI_CALL(cuptiActivityFlushAll(0));

    // disable unified memory counter activity
    CUPTI_CALL(cuptiActivityDisable(CUPTI_ACTIVITY_KIND_UNIFIED_MEMORY_COUNTER));
});
}

namespace cali
{

CaliperService cuptiuvm_service = { "cuptiuvm", initialize_uvm_callbacks };

}

// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// CuptiEventSampling.h
// Implementation of Cupti event sampling

#pragma once

#include "caliper/common/Attribute.h"

#include <cupti.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

namespace cali
{

class Caliper;
class SnapshotRecord;
    
namespace Cupti
{

class EventSampling
{
    struct SamplingInfo {
        CUcontext        context;
        CUpti_EventGroup event_grp;
    };

    std::vector<SamplingInfo> m_sampling_info;
    std::mutex                m_sampling_mtx;
    
    CUpti_EventID m_event_id;

    Attribute     m_event_attr    = Attribute::invalid;

    unsigned      m_num_snapshots = 0;
    unsigned      m_num_reads     = 0;

    bool          m_enabled       = false;
    
public:

    bool is_enabled() const { return m_enabled; }

    // note: event-by-name lookup is currently broken for some reason
    bool setup(Caliper* c, const std::string& event_name);
    
    bool setup(Caliper* c, CUpti_EventID event_id);

    bool enable_sampling_for_context(CUcontext context);
    bool disable_sampling_for_context(CUcontext context);

    void stop_all();

    void snapshot(Caliper* c, const SnapshotRecord*, SnapshotRecord* snapshot);

    std::ostream&
    print_statistics(std::ostream& os);

    EventSampling();

    ~EventSampling();    
};
    
} // namespace Cupti
    
} // namespace cali

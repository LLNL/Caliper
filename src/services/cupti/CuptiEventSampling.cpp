// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "CuptiEventSampling.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"

using namespace cali;
using namespace cali::Cupti;

namespace
{

void
print_cupti_error(std::ostream& os, CUptiResult err, const char* func)
{
    const char* errstr;

    cuptiGetResultString(err, &errstr);

    os << func << ": error: " << errstr << std::endl;
}

#define CHECK_CUPTI_ERR(err, cufunc)                        \
    if (err != CUPTI_SUCCESS) {                             \
        ::print_cupti_error(Log(0).stream() << "cupti-sampling: error: ", err, cufunc); \
        return false;                                       \
    }

bool
cleanup_cupti_event_group(CUpti_EventGroup group)
{
    CUptiResult res;

    res = cuptiEventGroupDisable(group);
    CHECK_CUPTI_ERR(res, "cuptiEventGroupDisable");

    res = cuptiEventGroupDestroy(group);
    CHECK_CUPTI_ERR(res, "cuptiEventGroupDestroy");

    return true;
}
    
// Iterate over all CUpti event domains and events to
//   find the event with the given name
    
// NOTE: This is currently broken: we get 'wrong'(?) event IDs.
// They work with cuptiEventGetAttribute in this function, but not in
// cuptiEventGroupAddEvent later on.
// The Event IDs we get here are different from the ones that
// the cupti_query program gets. No idea why. Those are the IDs that work.
// That app uses the device-specific cuptiDeviceEnumEventDomains etc.
// functions, so maybe it's that. 
    
bool
find_event_by_name(const std::string& target_evt_name, CUpti_EventID* ret_evt_id)
{
    CUptiResult res;
    
    uint32_t    num_domains = 0;
    
    res = cuptiGetNumEventDomains(&num_domains);
    CHECK_CUPTI_ERR(res, "cuptiGetNumEventDomains");

    std::vector<CUpti_EventDomainID> evt_domains(num_domains);
    size_t dom_buf_size = evt_domains.size() * sizeof(CUpti_EventDomainID);

    res = cuptiEnumEventDomains(&dom_buf_size, evt_domains.data());
    CHECK_CUPTI_ERR(res, "cuptiEnumEventDomains");

    for (CUpti_EventDomainID domain_id : evt_domains) {
        uint32_t num_events = 0;

        res = cuptiEventDomainGetNumEvents(domain_id, &num_events);
        CHECK_CUPTI_ERR(res, "cuptiEventDomainGetNumEvents");

        if (Log::verbosity() >= 2) {
            char   name[80];
            size_t size = sizeof(name);

            res = cuptiEventDomainGetAttribute(domain_id, CUPTI_EVENT_DOMAIN_ATTR_NAME, &size, name);
            CHECK_CUPTI_ERR(res, "cuptiEventDomainGetAttribute");

            name[79] = '\0';

            Log(2).stream() << "cupti-sampling: Looking for event \""
                            << target_evt_name
                            << "\" in event domain \"" << name << "\""
                            << std::endl;
        }

        std::vector<CUpti_EventID> events(num_events);
        size_t event_buf_size = events.size() * sizeof(CUpti_EventID);

        res = cuptiEventDomainEnumEvents(domain_id, &event_buf_size, events.data());
        CHECK_CUPTI_ERR(res, "cuptiEventDomainEnumEvents");

        for (CUpti_EventID evt_id : events) {
            char   name[80];
            size_t size = sizeof(name);

            res = cuptiEventGetAttribute(evt_id, CUPTI_EVENT_ATTR_NAME, &size, name);
            CHECK_CUPTI_ERR(res, "cuptiEventGetAttribute");

            name[79] = '\0';

            Log(2).stream() << "cupti-sampling:   Event " << evt_id << ": " << name << std::endl;

            if (target_evt_name == name) {
                Log(2).stream() << "cupti-sampling:   Found event ID "
                                << evt_id << std::endl;
                
                *ret_evt_id = evt_id;
                return true;
            }
        }
    }

    return false;
}
    
} // namespace [anonymous]


EventSampling::EventSampling()
    : m_enabled { false }
{ }

EventSampling::~EventSampling()
{
    stop_all();
}


bool
EventSampling::setup(Caliper* c, const std::string& event_name)
{    
    if (!::find_event_by_name(event_name, &m_event_id)) {
        Log(0).stream() << "cupti-sampling: CUpti event \"" << event_name
                        << "\" not found. Disabling CUpti event sampling."
                        << std::endl;
        return false;
    }

    Log(1).stream() << "cupti-sampling: Found CUpti event \""
                    << event_name
                    << "\" (ID = " << m_event_id << ")"
                    << std::endl;

    Attribute aggr_class_attr = c->get_attribute("class.aggregatable");
    Variant   v_true(true);

    m_event_attr =
        c->create_attribute(std::string("cupti.event.")+event_name, CALI_TYPE_UINT,
                            CALI_ATTR_ASVALUE,
                            1, &aggr_class_attr, &v_true);

    m_enabled = true;

    return true;
}

bool
EventSampling::setup(Caliper* c, CUpti_EventID event_id)
{
    CUptiResult res;

    m_event_id = event_id;

    char   name[80];
    size_t size = sizeof(name);
    
    res = cuptiEventGetAttribute(event_id, CUPTI_EVENT_ATTR_NAME, &size, name);
    CHECK_CUPTI_ERR(res, "cuptiEventGetAttribute");

    name[79] = '\0';

    Log(1).stream() << "cupti-sampling: Using CUpti event \""
                    << name
                    << "\" (ID = " << m_event_id << ")"
                    << std::endl;

    Attribute aggr_class_attr = c->get_attribute("class.aggregatable");
    Variant   v_true(true);

    m_event_attr =
        c->create_attribute(std::string("cupti.event.")+name, CALI_TYPE_UINT,
                            CALI_ATTR_ASVALUE,
                            1, &aggr_class_attr, &v_true);

    m_enabled = true;

    return true;
}

bool
EventSampling::enable_sampling_for_context(CUcontext context)
{
    SamplingInfo info;
    CUptiResult  res;

    info.context = context;

    if (Log::verbosity() >= 2) {
        uint32_t context_id;
        res = cuptiGetContextId(context, &context_id);
        CHECK_CUPTI_ERR(res, "cuptiGetContextId");
        
        Log(2).stream() << "cupti-sampling: Creating event group on context "
                        << context_id
                        << " for event " << m_event_id
                        << std::endl;
    }

    res = cuptiSetEventCollectionMode(context, CUPTI_EVENT_COLLECTION_MODE_CONTINUOUS);
    CHECK_CUPTI_ERR(res, "cuptiSetEventCollectionMode");

    res = cuptiEventGroupCreate(context, &info.event_grp, 0);
    CHECK_CUPTI_ERR(res, "cuptiEventGroupCreate");

    res = cuptiEventGroupAddEvent(info.event_grp, m_event_id);
    CHECK_CUPTI_ERR(res, "cuptiEventGroupAddEvent");

    res = cuptiEventGroupEnable(info.event_grp);
    CHECK_CUPTI_ERR(res, "cuptiEventGroupEnable");

    {
        std::lock_guard<std::mutex>
            g(m_sampling_mtx);
        
        m_sampling_info.push_back(info);
    }

    if (Log::verbosity() >= 2) {
        uint32_t context_id;
        res = cuptiGetContextId(context, &context_id);
        CHECK_CUPTI_ERR(res, "cuptiGetContextId");
        
        Log(2).stream() << "cupti-sampling: Started event sampling on context "
                        << context_id
                        << std::endl;
    }
    
    return true;
}

bool
EventSampling::disable_sampling_for_context(CUcontext context)
{
    auto it = std::find_if(m_sampling_info.begin(), m_sampling_info.end(),
                           [context](const SamplingInfo& info) {
                               return info.context == context;
                           } );

    if (it == m_sampling_info.end()) {
        Log(0).stream() << "cupti-sampling: stop sampling for unknown context "
                        << context << std::endl;
        return false;
    }

    cleanup_cupti_event_group(it->event_grp);

    m_sampling_info.erase(it);

    return true;
}

void
EventSampling::stop_all()
{
    for (SamplingInfo &info : m_sampling_info)
        cleanup_cupti_event_group(info.event_grp);

    m_sampling_info.clear();
}

void
EventSampling::snapshot(Caliper* c, const SnapshotRecord*, SnapshotRecord* snapshot)
{
    ++m_num_snapshots;

    CUpti_EventGroup group;

    //   This is a bit iffy here, we just attempt to read the event group
    // for the last CUDA context created and hope for the best. This works
    // when only one thread runs CUDA, which should be most cases.
    //   For a fix we need to figure out the actual current context, but it
    // seems we can only do that with the driver API. 

    {
        std::lock_guard<std::mutex>
            g(m_sampling_mtx);

        if (m_sampling_info.empty())
            return;

        group = m_sampling_info.back().event_grp;
    }

    uint64_t val;
    size_t   bytes_read;

    CUptiResult res =
        cuptiEventGroupReadEvent(group,
                                 CUPTI_EVENT_READ_FLAG_NONE,
                                 m_event_id,
                                 &bytes_read,
                                 &val);

    if (res != CUPTI_SUCCESS || bytes_read != sizeof(val))
        return;

    snapshot->append(m_event_attr.id(), Variant(cali_make_variant_from_uint(val)));
    
    ++m_num_reads;
}

std::ostream&
EventSampling::print_statistics(std::ostream& os)
{
    os << "cupti-sampling: "
       << m_num_snapshots << " total snapshots, "
       << m_num_reads << " cupti events read."
       << std::endl;

    return os;
}

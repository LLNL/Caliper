// Copyright (c) 2015-2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Papi.cpp
// PAPI provider for caliper records

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Log.h"

#include "caliper/common/util/spinlock.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <vector>

using namespace cali;

#include <papi.h>

namespace cali
{

extern cali::Attribute class_aggregatable_attr;

}

namespace
{

#define MAX_COUNTERS 32

void print_papi_error(const char* function, int code)
{
    Log(0).stream() << "papi: Error: " << function << ": " << PAPI_strerror(code) << std::endl;
}


class PapiService
{
    struct event_group_t {
        std::vector<int> codes;
        std::vector<cali_id_t> attrs;
    };

    typedef std::map< int, std::shared_ptr<event_group_t> >
        eventset_map_t;

    struct ThreadInfo {
        eventset_map_t eventsets;

        ThreadInfo*    next;
        ThreadInfo*    prev;

        void unlink() {
            if (next)
                next->prev = prev;
            if (prev)
                prev->next = next;
        }

        ThreadInfo()
            : next(nullptr), prev(nullptr)
            { }
    };

    Attribute m_thread_attr;

    unsigned  m_num_eventsets;
    unsigned  m_num_event_mismatch;
    unsigned  m_num_failed_acquire;
    unsigned  m_num_failed_read;
    unsigned  m_num_failed_reset;
    unsigned  m_num_failed_start;
    unsigned  m_num_threads;

    // PAPI component id -> event group info map for constructing the per-thread PAPI EventSets
    eventset_map_t m_event_groups;

    ThreadInfo*    m_thread_list;
    util::spinlock m_thread_lock;

    static int s_num_instances;

    static const ConfigSet::Entry s_configdata[];


    bool setup_event_info(Caliper* c, const std::vector<std::string>& eventlist) {
        m_event_groups.clear();

        Variant v_true(true);
        int count = 0;

        for (auto &name : eventlist) {
            if (count >= MAX_COUNTERS) {
                Log(0).stream() << "papi: Maximum number of counters reached, skipping "
                                << name << std::endl;
                continue;
            }

            char namebuf[1024];

            memset(namebuf, 0, sizeof(namebuf));
            strncpy(namebuf, name.c_str(), 1023);

            if (PAPI_query_named_event(namebuf) != PAPI_OK) {
                Log(0).stream() << "papi: Unknown event " << namebuf << std::endl;
                continue;
            }

            int code = PAPI_NULL;
            int ret  = PAPI_event_name_to_code(namebuf, &code);
            if (ret != PAPI_OK) {
                print_papi_error("PAPI_event_name_to_code()", ret);
                continue;
            }

            Attribute attr =
                c->create_attribute(std::string("papi.")+name, CALI_TYPE_UINT,
                                    CALI_ATTR_SCOPE_THREAD |
                                    CALI_ATTR_SKIP_EVENTS  |
                                    CALI_ATTR_ASVALUE,
                                    1, &class_aggregatable_attr, &v_true);

            int component = PAPI_get_event_component(code);

            // find or create eventset info for this component
            auto it = m_event_groups.find(component);
            if (it == m_event_groups.end()) {
                auto p = m_event_groups.emplace(std::make_pair(component, std::shared_ptr<event_group_t>(new event_group_t)));
                it = p.first;
            }

            it->second->codes.push_back(code);
            it->second->attrs.push_back(attr.id());

            ++count;
        }

        bool all_found = (static_cast<int>(eventlist.size()) == count);

        Log(2).stream() << "papi: Found "      << count
                        << " event codes for " << m_event_groups.size()
                        << " PAPI component(s)"
                        << std::endl;

        if (!all_found)
            Log(0).stream() << "papi: Unable to process all requested counters"
                            << std::endl;

        return all_found;
    }

    bool setup_thread_eventsets(Caliper* c) {
        std::map< int, std::shared_ptr<event_group_t> > eventsets;
        bool ok = true;

        for (auto &p : m_event_groups) {
            const PAPI_component_info_t* cpi = PAPI_get_component_info(p.first);

            if (Log::verbosity() >= 2) {
                Log(2).stream() << "papi: Creating eventset with " << p.second->codes.size()
                                << " events for component " << p.first
                                << " (" << (cpi ? cpi->name : "UKNOWN COMPONENT") << ")"
                                << std::endl;
            }

            int eventset = PAPI_NULL;

            int ret = PAPI_create_eventset(&eventset);
            if (ret != PAPI_OK) {
                print_papi_error("PAPI_create_eventset()", ret);
                ok = false;
                break;
            }

            int num = static_cast<int>(p.second->codes.size());

            if (cpi && num > 4 /* magic number for Intel counter support :-( */) {
                if (Log::verbosity() >= 2)
                    Log(2).stream() << "papi: Initializing multiplex support for component "
                                    << p.first << " (" << cpi->name << ")"
                                    << std::endl;

                ret = PAPI_assign_eventset_component(eventset, p.first);
                if (ret != PAPI_OK)
                    print_papi_error("PAPI_assign_eventset_component", ret);
                ret = PAPI_set_multiplex(eventset);
                if (ret != PAPI_OK)
                    print_papi_error("PAPI_set_multiplex", ret);
            }

            ret = PAPI_add_events(eventset, p.second->codes.data(), num);
            if (ret < 0) {
                print_papi_error("PAPI_add_events()", ret);

                PAPI_destroy_eventset(&eventset);
                ok = false;
                break;
            } else if (ret > 0 && ret < num) {
                Log(0).stream() << "papi: Added " << ret << " of " << num
                                << " events for component " << p.first
                                << " (" << (cpi ? cpi->name : "UKNOWN COMPONENT")
                                << "), skipping " << (num-ret)
                                << std::endl;
            }

            eventsets.emplace(std::make_pair(eventset, p.second));
        }

        if (ok) {
            ThreadInfo* td = new ThreadInfo;
            td->eventsets = std::move(eventsets);

            {
                std::lock_guard<util::spinlock>
                    g(m_thread_lock);

                if (m_thread_list)
                    m_thread_list->prev = td;

                td->next = m_thread_list;
                m_thread_list = td;

                m_num_eventsets += static_cast<int>(td->eventsets.size());
                ++m_num_threads;
            }

            c->set(m_thread_attr, Variant(cali_make_variant_from_ptr(td)));
        } else {
            for (const auto &p: eventsets) {
                int eventset = p.first;
                PAPI_destroy_eventset(&eventset);
            }
        }

        return ok;
    }

    ThreadInfo*
    get_thread_info(Caliper* c) {
        Entry e = c->get(m_thread_attr);

        if (e.is_empty())
            return nullptr;

        return static_cast<ThreadInfo*>(e.value().get_ptr());
    }

    void read_events(int eventset, const event_group_t& grp, SnapshotRecord* rec) {
        long long values[MAX_COUNTERS];

        int ret = PAPI_read(eventset, values);
        if (ret != PAPI_OK) {
            ++m_num_failed_read;
            return;
        }

        ret = PAPI_reset(eventset);
        if (ret != PAPI_OK) {
            ++m_num_failed_reset;
        }

        int count = PAPI_num_events(eventset);
        if (count == 0 || count > static_cast<int>(grp.attrs.size())) {
            ++m_num_event_mismatch;
            return;
        }

        for (int i = 0; i < count; ++i)
            rec->append(grp.attrs[i], Variant(cali_make_variant_from_uint(values[i])));
    }

    bool
    start_thread_counting(Caliper* c) {
        ThreadInfo* td = get_thread_info(c);

        if (!td) {
            ++m_num_failed_acquire;
            return false;
        }

        for (const auto &p : td->eventsets) {
            int ret = PAPI_start(p.first);
            if (ret != PAPI_OK) {
                ++m_num_failed_start;
                return false;
            }
        }

        return true;
    }

    void finish_eventset(int eventset) {
        int state = PAPI_NULL;
        int ret = PAPI_state(eventset, &state);
        if (ret != PAPI_OK) {
            print_papi_error("PAPI_state()", ret);
            return;
        }

        if (state & PAPI_RUNNING) {
            long long tmp[MAX_COUNTERS];
            ret = PAPI_stop(eventset, tmp);

            if (ret != PAPI_OK)
                print_papi_error("PAPI_stop()", ret);
        }

        ret = PAPI_cleanup_eventset(eventset);
        if (ret != PAPI_OK) {
            print_papi_error("PAPI_cleanup_eventset(): ", ret);
        }

        ret = PAPI_destroy_eventset(&eventset);
        if (ret != PAPI_OK) {
            print_papi_error("PAPI_destroy_eventset()", ret);
        }
    }

    void
    finish_thread_eventsets(Caliper* c) {
        ThreadInfo* td = get_thread_info(c);

        if (!td) {
            ++m_num_failed_acquire;
            return;
        }

        for (const auto &p : td->eventsets)
            finish_eventset(p.first);

        c->end(m_thread_attr);

        {
            std::lock_guard<util::spinlock>
                g(m_thread_lock);

            ThreadInfo* tmp = td->next;
            td->unlink();

            if (td == m_thread_list)
                m_thread_list = tmp;

            delete td;
        }
    }

    void
    snapshot(Caliper* c, SnapshotRecord* rec) {
        ThreadInfo* td = get_thread_info(c);

        if (!td)
            ++m_num_failed_acquire;

        for (const auto& p : td->eventsets)
            read_events(p.first, *(p.second), rec);
    }

    void
    finish(Caliper* c, Channel* channel) {
        Log(1).stream() << channel->name() << ": papi: Finishing" << std::endl;

        finish_thread_eventsets(c);

        unsigned errors =
            m_num_event_mismatch +
            m_num_failed_acquire +
            m_num_failed_read    +
            m_num_failed_reset   +
            m_num_failed_start;

        if (errors > 0)
            Log(0).stream() << channel->name() << ": papi: "
                            << m_num_failed_acquire << " failed thread data accesses, "
                            << m_num_failed_read    << " failed reads, "
                            << m_num_failed_reset   << " failed resets, "
                            << m_num_failed_start   << " failed starts, "
                            << m_num_event_mismatch << " event count mismatches."
                            << std::endl;

        Log(1).stream() << channel->name() << ": papi: Created "
                        << m_num_eventsets << " PAPI event set(s) on "
                        << m_num_threads   << " thread(s)."
                        << std::endl;
    }

    PapiService(Caliper* c, Channel* channel)
        : m_num_eventsets(0),
          m_num_event_mismatch(0),
          m_num_failed_acquire(0),
          m_num_failed_read(0),
          m_num_failed_reset(0),
          m_num_failed_start(0),
          m_num_threads(0),
          m_thread_list(nullptr)
        {
            m_thread_attr =
                c->create_attribute(std::string("papi.data.")+std::to_string(channel->id()),
                                    CALI_TYPE_PTR, CALI_ATTR_SCOPE_THREAD |
                                                   CALI_ATTR_HIDDEN       |
                                                   CALI_ATTR_ASVALUE      |
                                                   CALI_ATTR_SKIP_EVENTS);
        }


    static bool init_papi_library() {
        if (PAPI_is_initialized() == PAPI_THREAD_LEVEL_INITED)
            return true;

        int ret = PAPI_library_init(PAPI_VER_CURRENT);

        if (ret != PAPI_VER_CURRENT && ret > 0) {
            Log(0).stream() << "papi: PAPI version mismatch: found "
                            << ret << ", expected " << PAPI_VER_CURRENT << std::endl;
            return false;
        }

        PAPI_multiplex_init();
        PAPI_thread_init(pthread_self);

        if (PAPI_is_initialized() == PAPI_NOT_INITED) {
            Log(0).stream() << "papi: PAPI library could not be initialized" << std::endl;
            return false;
        }

        return true;
    }

    static void finish_papi_library() {
        if (--s_num_instances == 0) {
            Log(1).stream() << "papi: Shutdown" << std::endl;
            PAPI_shutdown();
        }
    }


public:

    ~PapiService() {
        // clear all trace buffers
        for (ThreadInfo* td = m_thread_list, *tmp = nullptr; td; td = tmp) {
            tmp = td->next;
            delete td;
        }
    }

    static void register_papi(Caliper* c, Channel* channel) {
        auto eventlist =
            channel->config().init("papi", s_configdata).get("counters").to_stringlist(",");

        if (eventlist.empty()) {
            Log(1).stream() << channel->name()
                            << ": papi: No counters specified, dropping papi service"
                            << std::endl;
            return;
        }

        if (!init_papi_library()) {
            Log(0).stream() << channel->name()
                            << ": papi: PAPI library not initialized, dropping papi service"
                            << std::endl;
            return;
        }

        ++s_num_instances;
        PapiService* instance = new PapiService(c, channel);

        if (!(instance->setup_event_info(c, eventlist) && instance->setup_thread_eventsets(c))) {
            Log(0).stream() << channel->name()
                            << ": papi: Failed to initialize event sets, dropping papi service"
                            << std::endl;

            finish_papi_library();
            return;
        }

        channel->events().post_init_evt.connect(
            [instance](Caliper* c, Channel*){
                instance->start_thread_counting(c);
            });
        channel->events().create_thread_evt.connect(
            [instance](Caliper* c, Channel*){
                if (instance->setup_thread_eventsets(c))
                    instance->start_thread_counting(c);
            });
        channel->events().release_thread_evt.connect(
            [instance](Caliper* c, Channel*){
                instance->finish_thread_eventsets(c);
            });
        channel->events().snapshot.connect(
            [instance](Caliper* c, Channel*, int, const SnapshotRecord*, SnapshotRecord* rec){
                instance->snapshot(c, rec);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish(c, channel);
                finish_papi_library();
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered papi service" << std::endl;
    }
};

int PapiService::s_num_instances = 0;

const ConfigSet::Entry PapiService::s_configdata[] = {
    { "counters", CALI_TYPE_STRING, "",
      "List of PAPI events to record",
      "List of PAPI events to record, separated by ','"
    },
    ConfigSet::Terminator
};

} // namespace


namespace cali
{

CaliperService papi_service = { "papi", ::PapiService::register_papi };

} // namespace cali

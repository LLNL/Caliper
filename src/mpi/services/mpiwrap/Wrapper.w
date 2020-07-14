// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "MpiEvents.h"
#include "services/mpiwrap/MpiTracing.h"

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/StringConverter.h"
#include "caliper/common/Variant.h"

#include <mpi.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <numeric>
#include <string>

#if defined(__GNUC__)
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace cali
{

extern Attribute mpifn_attr;
extern Attribute mpirank_attr;
extern Attribute mpisize_attr;
extern Attribute mpi_call_id_attr;

}

using namespace cali;
using namespace std;

namespace
{

bool enable_wrapper     = false;

// (Foo)_is_wrapped indicates wether the gotcha wrapper for (Foo) is active.
// (Foo)_wrap_count indicates how many current channels need (Foo) wrapped.
{{forallfn foo}}
bool {{foo}}_is_wrapped = false;
int  {{foo}}_wrap_count = 0;
{{endforallfn}}

inline void push_mpifn(Caliper* c, bool enabled, const char* fname)
{
    static std::atomic<uint64_t> call_id { 0 };

    if (!enabled)
        return;

    c->begin(mpi_call_id_attr, Variant(cali_make_variant_from_uint(++call_id)));
    c->begin(mpifn_attr, Variant(fname));
}

inline void pop_mpifn(Caliper* c, bool enabled)
{
    if (!enabled)
        return;

    c->end(mpifn_attr);
    c->end(mpi_call_id_attr);
}


struct MpiWrapperConfig
{
    static ConfigSet::Entry  s_configdata[];

    static MpiWrapperConfig* s_mwcs;

    //
    // The list of MPI configs
    //

    static MpiWrapperConfig* get_wrapper_config() {
        return s_mwcs;
    }

    static MpiWrapperConfig* get_wrapper_config(Channel* chn) {
        MpiWrapperConfig* mwc = s_mwcs;

        while (mwc && mwc->channel->id() != chn->id())
            mwc = mwc->next;

        if (!mwc) {
            mwc = new MpiWrapperConfig(chn);

            if (s_mwcs)
                s_mwcs->prev = mwc;

            mwc->next = s_mwcs;
            s_mwcs = mwc;
        }

        return mwc;
    }

    static void delete_wrapper_config(Channel* chn) {
        MpiWrapperConfig* mwc = s_mwcs;

        while (mwc && mwc->channel->id() != chn->id())
            mwc = mwc->next;

        if (mwc) {
            if (mwc == s_mwcs)
                s_mwcs = mwc->next;

            mwc->unlink();
            delete mwc;
        }
    }

    // Constructor / destructor
    //

    MpiWrapperConfig(Channel* chn)
        : channel(chn), next(nullptr), prev(nullptr)
        {
            ConfigSet cfg = chn->config().init("mpi", s_configdata);

            setup_filter(cfg.get("whitelist").to_string(), cfg.get("blacklist").to_string());
            enable_msg_tracing = cfg.get("msg_tracing").to_bool();
        }

    ~MpiWrapperConfig()
        { }

    // Per-channel variables
    //

    MpiEvents   mpi_events;

    Channel*    channel = nullptr;

    bool        enable_msg_tracing;
    MpiTracing  tracing;

    MpiWrapperConfig* next;
    MpiWrapperConfig* prev;

    {{forallfn foo}}
    bool enable_{{foo}} = false;
    {{endforallfn}}

    // Helper functions
    //

    void unlink() {
        if (next)
            next->prev = prev;
        if (prev)
            prev->next = next;
    }

    void inc_wrap_counters() {
        {{forallfn foo}}
        if (enable_{{foo}})
            ++{{foo}}_wrap_count;
        {{endforallfn}}
    }

    void dec_wrap_counters() {
        {{forallfn foo}}
        if (enable_{{foo}})
            --{{foo}}_wrap_count;
        {{endforallfn}}
    }

    void setup_filter(const std::string& whitelist_string, const std::string& blacklist_string) {
        std::vector<std::string> whitelist =
            StringConverter(whitelist_string).to_stringlist(",:");
        std::vector<std::string> blacklist =
            StringConverter(blacklist_string).to_stringlist(",:");

        bool have_whitelist = whitelist.size() > 0;
        bool have_blacklist = blacklist.size() > 0;

        if (!have_whitelist && !have_blacklist)
            return;

        bool enable_all = false;

        if (have_whitelist && whitelist.front() == "all") {
            enable_all = true;
            whitelist.erase(whitelist.begin());
        }

        const struct fntable_elem {
            const char* name;
            bool*       enableptr;
        } table[] = {
            {{forallfn foo}}
            { "{{foo}}", &enable_{{foo}} },
            {{endforallfn}}
            { 0, 0 }
        };

        for (const fntable_elem* e = table; e->name && e->enableptr; ++e) {
            if (enable_all)
                *(e->enableptr) = true;

            std::string fnstr(e->name);

            if (have_whitelist) {
                vector<string>::iterator it = std::find(whitelist.begin(), whitelist.end(), fnstr);

                if (it != whitelist.end()) {
                    *(e->enableptr) = true;
                    whitelist.erase(it);
                }
            }
            if (have_blacklist) {
                vector<string>::iterator it = std::find(blacklist.begin(), blacklist.end(), fnstr);

                if (it != blacklist.end()) {
                    blacklist.erase(it);
                    *(e->enableptr) = false;
                } else if (!have_whitelist) {
                    *(e->enableptr) = true;
                }
            }
        }

        for (vector<string>::const_iterator it = whitelist.begin(); it != whitelist.end(); ++it)
            Log(1).stream() << "Unknown MPI function " << *it << " in MPI function whitelist" << endl;
        for (vector<string>::const_iterator it = blacklist.begin(); it != blacklist.end(); ++it)
            Log(1).stream() << "Unknown MPI function " << *it << " in MPI function blacklist" << endl;
    }
}; // struct MpiWrapperConfig

MpiWrapperConfig* MpiWrapperConfig::s_mwcs = nullptr;

ConfigSet::Entry MpiWrapperConfig::s_configdata[] = {
    { "whitelist", CALI_TYPE_STRING, "",
      "List of MPI functions to instrument",
      "Colon-separated list of MPI functions to instrument.\n"
      "If set, the whitelisted MPI functions will be instrumented."
    },
    { "blacklist", CALI_TYPE_STRING, "",
      "List of MPI functions to filter",
      "Colon-separated list of functions to blacklist."
    },
    { "msg_tracing", CALI_TYPE_BOOL, "false",
      "Enable MPI message tracing",
      "Enable MPI message tracing"
    },
    ConfigSet::Terminator
};


void
mpi_init_cb(Caliper* c, Channel* chn)
{
    int rank = -1, size = -1;

    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_size(MPI_COMM_WORLD, &size);

    c->set(mpirank_attr, Variant(rank));
    c->set(mpisize_attr, Variant(size));

    MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(chn);

    if (mwc->enable_msg_tracing) {
        Log(2).stream() << chn->name() << ": Enabling MPI message tracing" << std::endl;
        mwc->tracing.init_mpi(c, chn);
    }
}

void
post_init_cb(Caliper* c, Channel* channel)
{
    channel->events().subscribe_attribute(c, channel, mpifn_attr);

    int initialized = 0;
    int finalized   = 0;

    PMPI_Initialized(&initialized);
    PMPI_Finalized(&finalized);

    if (initialized && !finalized)
        MpiWrapperConfig::get_wrapper_config(channel)->mpi_events.mpi_init_evt(c, channel);
}

} // namespace [anonymous]


//
// --- Point-to-Point
//

{{fn func MPI_Send MPI_Bsend MPI_Rsend MPI_Ssend MPI_Isend MPI_Ibsend MPI_Irsend MPI_Issend}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_{{func}} && mwc->channel->is_active() && mwc->enable_msg_tracing)
                    mwc->tracing.handle_send(&c, mwc->channel, {{1}}, {{2}}, {{3}}, {{4}}, {{5}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Send_init MPI_Bsend_init MPI_Rsend_init MPI_Ssend_init}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_send_init(&c, mwc->channel, {{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Recv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        MPI_Status tmp_status;

        if ({{6}} == MPI_STATUS_IGNORE)
            {{6}} = &tmp_status;

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_recv(&c, mwc->channel, {{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Sendrecv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        MPI_Status tmp_status;

        if ({{11}} == MPI_STATUS_IGNORE)
            {{11}} = &tmp_status;

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active()) {
                mwc->tracing.handle_send(&c, mwc->channel, {{1}}, {{2}}, {{3}}, {{4}}, {{10}});
                mwc->tracing.handle_recv(&c, mwc->channel, {{6}}, {{7}}, {{8}}, {{9}}, {{10}}, {{11}});
            }

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Sendrecv_replace}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        MPI_Status tmp_status;

        if ({{8}} == MPI_STATUS_IGNORE)
            {{8}} = &tmp_status;

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active()) {
                mwc->tracing.handle_send(&c, mwc->channel, {{1}}, {{2}}, {{3}}, {{4}}, {{7}});
                mwc->tracing.handle_recv(&c, mwc->channel, {{1}}, {{2}}, {{5}}, {{6}}, {{7}}, {{8}});
            }

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Irecv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_irecv(&c, mwc->channel, {{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Recv_init}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_recv_init(&c, mwc->channel, {{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Start}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_start(&c, mwc->channel, 1, {{0}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Startall}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_start(&c, mwc->channel, {{0}}, {{1}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Wait}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        MPI_Request tmp_req = *{{0}};
        MPI_Status  tmp_status;

        if ({{1}} == MPI_STATUS_IGNORE)
            {{1}} = &tmp_status;

        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_completion(&c, mwc->channel, 1, &tmp_req, {{1}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Waitall}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        int nreq = {{0}};

        MPI_Request* tmp_req      = nullptr;
        MPI_Status*  tmp_statuses = nullptr;

        bool any_msg_tracing = false;

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_{{func}} && mwc->channel->is_active() && mwc->enable_msg_tracing) {
                any_msg_tracing = true;
                break;
            }

        if (any_msg_tracing) {
            tmp_req      = new MPI_Request[nreq];
            tmp_statuses = new MPI_Status[nreq];

            std::copy_n({{1}}, nreq, tmp_req);

            if ({{2}} == MPI_STATUSES_IGNORE)
                {{2}} = tmp_statuses;
        }

        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && nreq > 0 && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_completion(&c, mwc->channel, nreq, tmp_req, {{2}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);

        if (any_msg_tracing) {
            delete[] tmp_statuses;
            delete[] tmp_req;
        }
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Waitany}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;

        int nreq = {{0}};

        MPI_Request* tmp_req = nullptr;
        MPI_Status   tmp_status;

        bool any_msg_tracing = false;

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_{{func}} && mwc->channel->is_active() && mwc->enable_msg_tracing) {
                any_msg_tracing = true;
                break;
            }

        if (any_msg_tracing) {
            tmp_req = new MPI_Request[nreq];
            std::copy_n({{1}}, nreq, tmp_req);

            if ({{3}} == MPI_STATUS_IGNORE)
                {{3}} = &tmp_status;
        }

        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && nreq > 0 && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_completion(&c, mwc->channel, 1, tmp_req+(*{{2}}), {{3}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);

        if (any_msg_tracing)
            delete[] tmp_req;
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Testsome MPI_Waitsome}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;

        int nreq = {{0}};

        MPI_Request* tmp_req = nullptr;
        MPI_Status*  tmp_statuses;

        bool any_msg_tracing = false;

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_{{func}} && mwc->channel->is_active() && mwc->enable_msg_tracing) {
                any_msg_tracing = true;
                break;
            }

        if (any_msg_tracing) {
            tmp_req      = new MPI_Request[nreq];
            tmp_statuses = new MPI_Status[nreq];

            std::copy_n({{1}}, nreq, tmp_req);

            if ({{4}} == MPI_STATUSES_IGNORE)
                {{4}} = tmp_statuses;
        }

        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                for (int i = 0; i < *{{2}}; ++i)
                    mwc->tracing.handle_completion(&c, mwc->channel, 1, tmp_req+{{3}}[i], {{4}}+{{3}}[i]);

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);

        if (any_msg_tracing) {
            delete[] tmp_statuses;
            delete[] tmp_req;
        }
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Test}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;

        MPI_Request tmp_req = *{{0}};
        MPI_Status  tmp_status;

        if ({{2}} == MPI_STATUS_IGNORE)
            {{2}} = &tmp_status;

        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && *{{1}} && mwc->enable_{{func}} && mwc->channel->is_active())
                    mwc->tracing.handle_completion(&c, mwc->channel, 1, &tmp_req, {{2}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Testall}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;

        int nreq = {{0}};

        MPI_Request* tmp_req      = nullptr;
        MPI_Status*  tmp_statuses = nullptr;

        bool any_msg_tracing = false;

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_{{func}} && mwc->channel->is_active() && mwc->enable_msg_tracing) {
                any_msg_tracing = true;
                break;
            }

        if (any_msg_tracing) {
            tmp_req      = new MPI_Request[nreq];
            tmp_statuses = new MPI_Status[nreq];

            std::copy_n({{1}}, nreq, tmp_req);

            if ({{3}} == MPI_STATUSES_IGNORE)
            {{3}} = tmp_statuses;
        }

        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && *{{2}} && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_completion(&c, mwc->channel, nreq, tmp_req, {{3}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);

        if (any_msg_tracing) {
            delete[] tmp_statuses;
            delete[] tmp_req;
        }
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Testany}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;

        int nreq = {{0}};

        MPI_Request* tmp_req = nullptr;
        MPI_Status   tmp_status;

        bool any_msg_tracing = false;

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_{{func}} && mwc->channel->is_active() && mwc->enable_msg_tracing) {
                any_msg_tracing = true;
                break;
            }

        if (any_msg_tracing) {
            tmp_req = new MPI_Request[nreq];
            std::copy_n({{1}}, nreq, tmp_req);

            if ({{4}} == MPI_STATUS_IGNORE)
                {{4}} = &tmp_status;
        }

        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && *{{3}} && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_completion(&c, mwc->channel, 1, tmp_req+(*{{2}}), {{4}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);

        if (any_msg_tracing)
            delete[] tmp_req;
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Request_free}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.request_free(&c, mwc->channel, {{0}});

        {{callfn}}

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

//
// --- Collectives
//

{{fn func MPI_Barrier}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_barrier(&c, mwc->channel, {{0}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Bcast}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_12n(&c, mwc->channel, {{1}}, {{2}}, {{3}}, {{4}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Scatter}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_12n(&c, mwc->channel, {{1}}, {{2}}, {{6}}, {{7}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Scatterv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active()) {
                int tmp_commsize = 0;
                PMPI_Comm_size({{8}}, &tmp_commsize);
                int total_count  = std::accumulate({{2}}, {{2}}+tmp_commsize, 0);

                mwc->tracing.handle_12n(&c, mwc->channel, total_count, {{3}}, {{7}}, {{8}});
            }

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Gather}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_n21(&c, mwc->channel, {{1}}, {{2}}, {{6}}, {{7}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Gatherv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_n21(&c, mwc->channel, {{1}}, {{2}}, {{7}}, {{8}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Reduce}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;

        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_n21(&c, mwc->channel, {{2}}, {{3}}, {{5}}, {{6}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Scan MPI_Exscan}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_n2n(&c, mwc->channel, {{2}}, {{3}}, {{5}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Reduce_scatter}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active()) {
                int tmp_rank = 0;
                PMPI_Comm_rank({{5}}, &tmp_rank);

                mwc->tracing.handle_n2n(&c, mwc->channel, {{2}}[tmp_rank], {{3}}, {{5}});
            }

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Allreduce}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_n2n(&c, mwc->channel, {{2}}, {{3}}, {{5}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Allgather}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_n2n(&c, mwc->channel, {{1}}, {{2}}, {{6}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Allgatherv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
                mwc->tracing.handle_n2n(&c, mwc->channel, {{1}}, {{2}}, {{7}});

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Alltoall}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active()) {
                int tmp_commsize = 0;
                PMPI_Comm_size({{6}}, &tmp_commsize);

                mwc->tracing.handle_n2n(&c, mwc->channel, tmp_commsize * {{1}}, {{2}}, {{6}});
            }

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Alltoallv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;

        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

        {{callfn}}

        for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
            if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active()) {
                int tmp_commsize = 0;
                PMPI_Comm_size({{8}}, &tmp_commsize);
                int total_count  = std::accumulate({{1}}, {{1}}+tmp_commsize, 0);

                mwc->tracing.handle_n2n(&c, mwc->channel, total_count, {{3}}, {{8}});
            }

        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

//
// --- Init/finalize
//

{{fn func MPI_Init MPI_Init_thread}}{
    {{callfn}}

    bool run_init_evts = Caliper::is_initialized();
    Caliper c;

    // cheat a bit: put begin/ends around a barrier here

    for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next) {
        //   Run mpi init events here if Caliper was initialized before MPI_Init
        // Otherwise they will run via the Caliper initialization above.
        if (run_init_evts)
            mwc->mpi_events.mpi_init_evt(&c, mwc->channel);
    }

    ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

    PMPI_Barrier(MPI_COMM_WORLD);

    for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
        if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
            mwc->tracing.handle_init(&c, mwc->channel);

    ::pop_mpifn(&c, ::{{func}}_wrap_count);
}{{endfn}}

{{fn func MPI_Finalize}}{
    Caliper c;

    // cheat a bit: put begin/ends around a barrier here

    ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");

    PMPI_Barrier(MPI_COMM_WORLD);

    for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
        if (mwc->enable_msg_tracing && mwc->enable_{{func}} && mwc->channel->is_active())
            mwc->tracing.handle_finalize(&c, mwc->channel);

    ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);

    for (MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(); mwc; mwc = mwc->next)
        mwc->mpi_events.mpi_finalize_evt(&c, mwc->channel);

    {{callfn}}
}{{endfn}}


//
// --- Generic wrapper for all other MPI functions
//

{{fnall func
    MPI_Init MPI_Init_thread
    MPI_Finalize
    MPI_Send  MPI_Bsend  MPI_Rsend  MPI_Ssend
    MPI_Isend MPI_Ibsend MPI_Irsend MPI_Issend
    MPI_Send_init MPI_Bsend_init MPI_Rsend_init MPI_Ssend_init
    MPI_Recv MPI_Irecv MPI_Recv_init
    MPI_Sendrecv MPI_Sendrecv_replace
    MPI_Start MPI_Startall MPI_Request_free
    MPI_Wait MPI_Waitall MPI_Waitany MPI_Waitsome
    MPI_Test MPI_Testall MPI_Testany MPI_Testsome
    MPI_Barrier
    MPI_Bcast MPI_Gather MPI_Scatter MPI_Reduce
    MPI_Allreduce MPI_Allgather MPI_Alltoall
    MPI_Reduce_scatter MPI_Scan MPI_Exscan
    MPI_Allgatherv MPI_Alltoallv MPI_Gatherv MPI_Scatterv
}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper) {
#endif
        Caliper c;
        ::push_mpifn(&c, ::{{func}}_wrap_count > 0, "{{func}}");
        {{callfn}}
        ::pop_mpifn(&c, ::{{func}}_wrap_count > 0);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfnall}}


//
// --- Wrapper initialization
//

namespace cali
{

// --- MpiEvents access
//

MpiEvents& mpiwrap_get_events(Channel* chn)
{
    return MpiWrapperConfig::get_wrapper_config(chn)->mpi_events;
}

//
// --- Init function
//

void mpiwrap_init(Caliper* c, Channel* chn)
{
    // --- register callbacks

    chn->events().post_init_evt.connect(::post_init_cb);

    chn->events().finish_evt.connect(
        [](Caliper* c, Channel* channel){
            Log(2).stream() << channel->name() << ": Finishing mpi service" << std::endl;
            MpiWrapperConfig::get_wrapper_config(channel)->dec_wrap_counters();
            MpiWrapperConfig::delete_wrapper_config(channel);
        });

    // --- setup wrappers

    ::enable_wrapper = true;

    MpiWrapperConfig* mwc = MpiWrapperConfig::get_wrapper_config(chn);

    mwc->mpi_events.mpi_init_evt.connect(::mpi_init_cb);

    if (mwc->enable_msg_tracing)
        mwc->tracing.init(c, chn);

#ifdef CALIPER_MPIWRAP_USE_GOTCHA
    Log(2).stream() << chn->name() << ": mpiwrap: Using GOTCHA wrappers." << std::endl;

    //   AWFUL HACK: gotcha can modify our bindings buffer later.
    // (It should copy them instead!)
    // Copy them to a large enough static buffer for now.

    static struct gotcha_binding_t s_bindings[1024];
    static size_t s_binding_pos = 0;

    std::vector<struct gotcha_binding_t> bindings;

    // we always wrap init & finalize
    if (!::MPI_Init_is_wrapped)
        bindings.push_back(wrap_MPI_Init_binding);
    if (!::MPI_Init_thread_is_wrapped)
        bindings.push_back(wrap_MPI_Init_thread_binding);
    if (!::MPI_Finalize_is_wrapped)
        bindings.push_back(wrap_MPI_Finalize_binding);

    ::MPI_Init_is_wrapped        = true;
    ::MPI_Init_thread_is_wrapped = true;
    ::MPI_Finalize_is_wrapped    = true;

    {{forallfn name MPI_Init MPI_Init_thread MPI_Finalize}}
    if (mwc->enable_{{name}} && !::{{name}}_is_wrapped) {
        bindings.push_back(wrap_{{name}}_binding);
        ::{{name}}_is_wrapped = true;
    }
    {{endforallfn}}

    std::copy_n(bindings.data(), bindings.size(), s_bindings+s_binding_pos);
    gotcha_wrap(s_bindings+s_binding_pos, bindings.size(), "caliper/mpi");
    s_binding_pos += bindings.size();
#else
    Log(2).stream() << chn->name() << ": mpiwrap: Using PMPI wrappers." << std::endl;
#endif

    mwc->inc_wrap_counters();
}

}

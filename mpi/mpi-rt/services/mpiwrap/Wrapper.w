#include "MpiEvents.h"
#include "services/mpiwrap/MpiTracing.h"

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"
#include "caliper/common/Variant.h"

#include <mpi.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <numeric>
#include <string>

namespace cali
{

extern Attribute   mpifn_attr;
extern Attribute   mpirank_attr;
extern Attribute   mpisize_attr;

extern bool        enable_msg_tracing;

}

using namespace cali;
using namespace std;

namespace 
{

bool       enable_wrapper = false;
MpiTracing tracing;

{{forallfn foo}}
bool enable_{{foo}} = false;
{{endforallfn}}

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

void
mpi_init_cb(Caliper* c)
{
    int rank = -1, size = -1;

    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_size(MPI_COMM_WORLD, &size);

    c->set(mpirank_attr, Variant(rank));
    c->set(mpisize_attr, Variant(size));

    ::tracing.init_mpi(c);
}

void
post_init_cb(Caliper* c)
{
    int initialized = 0;
    int finalized   = 0;

    PMPI_Initialized(&initialized);
    PMPI_Finalized(&finalized);

    if (initialized && !finalized)
        MpiEvents::events.mpi_init_evt(c);
}

} // namespace [anonymous]


//
// --- Point-to-Point
//

{{fn func MPI_Send MPI_Bsend MPI_Rsend MPI_Ssend MPI_Isend MPI_Ibsend MPI_Irsend MPI_Issend}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);
        
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));        

        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_send(&c, {{1}}, {{2}}, {{3}}, {{4}}, {{5}});

        c.end(mpifn_attr);

        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Send_init MPI_Bsend_init MPI_Rsend_init MPI_Ssend_init}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);
        
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));        

        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_send_init(&c, {{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}});

        c.end(mpifn_attr);

        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Recv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);
        
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));

        MPI_Status tmp_status;
        
        if ({{6}} == MPI_STATUS_IGNORE)
            {{6}} = &tmp_status;

        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_recv(&c, {{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}});
        
        c.end(mpifn_attr);

        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);        
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Sendrecv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);
        
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));

        MPI_Status tmp_status;
        
        if ({{11}} == MPI_STATUS_IGNORE)
            {{11}} = &tmp_status;

        {{callfn}}

        if (enable_msg_tracing) {
            ::tracing.handle_send(&c, {{1}}, {{2}}, {{3}}, {{4}}, {{10}});
            ::tracing.handle_recv(&c, {{6}}, {{7}}, {{8}}, {{9}}, {{10}}, {{11}});
        }
        
        c.end(mpifn_attr);

        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);        
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif    
}{{endfn}}

{{fn func MPI_Sendrecv_replace}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);
        
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));

        MPI_Status tmp_status;
        
        if ({{8}} == MPI_STATUS_IGNORE)
            {{8}} = &tmp_status;

        {{callfn}}

        if (enable_msg_tracing) {
            ::tracing.handle_send(&c, {{1}}, {{2}}, {{3}}, {{4}}, {{7}});
            ::tracing.handle_recv(&c, {{1}}, {{2}}, {{5}}, {{6}}, {{7}}, {{8}});
        }
        
        c.end(mpifn_attr);

        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);        
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif    
}{{endfn}}

{{fn func MPI_Irecv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);
        
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        
        if (enable_msg_tracing)
            ::tracing.handle_irecv(&c, {{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}});

        c.end(mpifn_attr);

        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Recv_init}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);
        
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        
        if (enable_msg_tracing)
            ::tracing.handle_recv_init(&c, {{1}}, {{2}}, {{3}}, {{4}}, {{5}}, {{6}});

        c.end(mpifn_attr);

        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Start}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        {{callfn}}
        
        if (enable_msg_tracing)
            ::tracing.handle_start(&c, 1, {{0}});

        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Startall}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        {{callfn}}
        
        if (enable_msg_tracing)
            ::tracing.handle_start(&c, {{0}}, {{1}});

        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Wait}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);
        
        MPI_Request tmp_req = *{{0}};
        MPI_Status  tmp_status;

        if ({{1}} == MPI_STATUS_IGNORE)
            {{1}} = &tmp_status;

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        {{callfn}}
        
        if (enable_msg_tracing)
            ::tracing.handle_completion(&c, 1, &tmp_req, {{1}});

        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Waitall}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        int nreq = {{0}};
        
        MPI_Request* tmp_req      = nullptr;
        MPI_Status*  tmp_statuses = nullptr;

        if (enable_msg_tracing) {
            ::tracing.push_call_id(&c);

            tmp_req      = new MPI_Request[nreq];
            tmp_statuses = new MPI_Status[nreq];

            std::copy_n({{1}}, nreq, tmp_req);

            if ({{2}} == MPI_STATUSES_IGNORE)
                {{2}} = tmp_statuses;
        }

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        {{callfn}}
        
        if (enable_msg_tracing)
            ::tracing.handle_completion(&c, nreq, tmp_req, {{2}});

        c.end(mpifn_attr);
        
        if (enable_msg_tracing) {
            delete[] tmp_statuses;
            delete[] tmp_req;

            ::tracing.pop_call_id(&c);
        }
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Waitany}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        int nreq = {{0}};
        
        MPI_Request* tmp_req = nullptr;
        MPI_Status   tmp_status;

        if (enable_msg_tracing) {
            ::tracing.push_call_id(&c);

            tmp_req = new MPI_Request[nreq];
            std::copy_n({{1}}, nreq, tmp_req);

            if ({{3}} == MPI_STATUS_IGNORE)
                {{3}} = &tmp_status;
        }

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        {{callfn}}
        
        if (enable_msg_tracing && nreq > 0)
            ::tracing.handle_completion(&c, 1, tmp_req+(*{{2}}), {{3}});

        c.end(mpifn_attr);
        
        if (enable_msg_tracing) {
            delete[] tmp_req;

            ::tracing.pop_call_id(&c);
        }
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Testsome MPI_Waitsome}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        int nreq = {{0}};
        
        MPI_Request* tmp_req = nullptr;
        MPI_Status*  tmp_statuses;

        if (enable_msg_tracing) {
            ::tracing.push_call_id(&c);

            tmp_req      = new MPI_Request[nreq];
            tmp_statuses = new MPI_Status[nreq];
                
            std::copy_n({{1}}, nreq, tmp_req);

            if ({{4}} == MPI_STATUSES_IGNORE)
                {{4}} = tmp_statuses;
        }

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        {{callfn}}
        
        if (enable_msg_tracing > 0)
            for (int i = 0; i < *{{2}}; ++i)
                ::tracing.handle_completion(&c, 1, tmp_req+{{3}}[i], {{4}}+{{3}}[i]);

        c.end(mpifn_attr);
        
        if (enable_msg_tracing) {
            delete[] tmp_statuses;
            delete[] tmp_req;

            ::tracing.pop_call_id(&c);
        }
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Test}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);
        
        MPI_Request tmp_req = *{{0}};
        MPI_Status  tmp_status;

        if ({{2}} == MPI_STATUS_IGNORE)
            {{2}} = &tmp_status;

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        {{callfn}}
        
        if (enable_msg_tracing && *{{1}})
            ::tracing.handle_completion(&c, 1, &tmp_req, {{2}});

        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Testall}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        int nreq = {{0}};
        
        MPI_Request* tmp_req      = nullptr;
        MPI_Status*  tmp_statuses = nullptr;

        if (enable_msg_tracing) {
            ::tracing.push_call_id(&c);

            tmp_req      = new MPI_Request[nreq];
            tmp_statuses = new MPI_Status[nreq];

            std::copy_n({{1}}, nreq, tmp_req);

            if ({{3}} == MPI_STATUSES_IGNORE)
                {{3}} = tmp_statuses;
        }

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        {{callfn}}
        
        if (enable_msg_tracing && *{{2}})
            ::tracing.handle_completion(&c, nreq, tmp_req, {{3}});

        c.end(mpifn_attr);
        
        if (enable_msg_tracing) {
            delete[] tmp_statuses;
            delete[] tmp_req;

            ::tracing.pop_call_id(&c);
        }
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Testany}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        int nreq = {{0}};
        
        MPI_Request* tmp_req = nullptr;
        MPI_Status   tmp_status;

        if (enable_msg_tracing) {
            ::tracing.push_call_id(&c);

            tmp_req = new MPI_Request[nreq];
            std::copy_n({{1}}, nreq, tmp_req);

            if ({{4}} == MPI_STATUS_IGNORE)
                {{4}} = &tmp_status;
        }

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        {{callfn}}
        
        if (enable_msg_tracing && *{{3}})
            ::tracing.handle_completion(&c, 1, tmp_req+(*{{2}}), {{4}});

        c.end(mpifn_attr);
        
        if (enable_msg_tracing) {
            delete[] tmp_req;

            ::tracing.pop_call_id(&c);
        }
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Request_free}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        
        if (enable_msg_tracing)
            ::tracing.request_free(&c, {{0}});

        {{callfn}}
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
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
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_barrier(&c, {{0}});

        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Bcast}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_12n(&c, {{1}}, {{2}}, {{3}}, {{4}});
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Scatter}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_12n(&c, {{1}}, {{2}}, {{6}}, {{7}});
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Scatterv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing) {
            int tmp_commsize = 0;
            PMPI_Comm_size({{8}}, &tmp_commsize);
            int total_count  = std::accumulate({{2}}, {{2}}+tmp_commsize, 0);
            
            ::tracing.handle_12n(&c, total_count, {{3}}, {{7}}, {{8}});
        }
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Gather}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_n21(&c, {{1}}, {{2}}, {{6}}, {{7}});
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Gatherv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_n21(&c, {{1}}, {{2}}, {{7}}, {{8}});
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Reduce}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_n21(&c, {{2}}, {{3}}, {{5}}, {{6}});
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Scan MPI_Exscan}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_n2n(&c, {{2}}, {{3}}, {{5}});
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Reduce_scatter}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing) {
            int tmp_rank = 0;
            PMPI_Comm_rank({{5}}, &tmp_rank);

            ::tracing.handle_n2n(&c, {{2}}[tmp_rank], {{3}}, {{5}});
        }
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Allreduce}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_n2n(&c, {{2}}, {{3}}, {{5}});
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Allgather}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_n2n(&c, {{1}}, {{2}}, {{6}});
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Allgatherv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}

        if (enable_msg_tracing)
            ::tracing.handle_n2n(&c, {{1}}, {{2}}, {{7}});
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Alltoall}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        
        if (enable_msg_tracing) {
            int tmp_commsize = 0;
            PMPI_Comm_size({{6}}, &tmp_commsize);

            ::tracing.handle_n2n(&c, tmp_commsize * {{1}}, {{2}}, {{6}});
        }
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    } else {
        {{callfn}}
    }
#endif
}{{endfn}}

{{fn func MPI_Alltoallv}}{
#ifndef CALIPER_MPIWRAP_USE_GOTCHA
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;

        if (enable_msg_tracing)
            ::tracing.push_call_id(&c);

        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        
        if (enable_msg_tracing) {
            int tmp_commsize = 0;
            PMPI_Comm_size({{8}}, &tmp_commsize);
            int total_count  = std::accumulate({{1}}, {{1}}+tmp_commsize, 0);

            ::tracing.handle_n2n(&c, total_count, {{3}}, {{8}});
        }
        
        c.end(mpifn_attr);
        
        if (enable_msg_tracing)
            ::tracing.pop_call_id(&c);
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

    if (Caliper::is_initialized()) {
        // MPI_Init after Caliper init: Invoke Caliper's mpi init event now.
        Caliper c;
        MpiEvents::events.mpi_init_evt(&c);
    } else {
        //   MPI_Init before Caliper init (PMPI case): Initialize Caliper here.
        // Invokes Caliper's mpi init event in post_init_cb.
        Caliper::instance();
    }
}{{endfn}}

{{fn func MPI_Finalize}}{
    Caliper c;
    MpiEvents::events.mpi_finalize_evt(&c);

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
    if (::enable_wrapper && ::enable_{{func}}) {
#endif
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
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

//
// --- Init function
//

void mpiwrap_init(Caliper* c, const std::string& whitelist, const std::string& blacklist)
{
    // --- register callbacks

    c->events().post_init_evt.connect(::post_init_cb);
    MpiEvents::events.mpi_init_evt.connect(::mpi_init_cb);

    // --- setup wrappers

    ::enable_wrapper = true;

    if (enable_msg_tracing)
        ::tracing.init(c);

    setup_filter(whitelist, blacklist);

#ifdef CALIPER_MPIWRAP_USE_GOTCHA
    Log(2).stream() << "mpiwrap: Using GOTCHA wrappers." << std::endl;

    std::vector<struct gotcha_binding_t> bindings;

    // we always wrap init & finalize
    bindings.push_back(wrap_MPI_Init_binding);
    bindings.push_back(wrap_MPI_Init_thread_binding);
    bindings.push_back(wrap_MPI_Finalize_binding);

    {{forallfn name MPI_Init MPI_Init_thread MPI_Finalize}}
    if (::enable_{{name}})
        bindings.push_back(wrap_{{name}}_binding);
    {{endforallfn}}

    gotcha_wrap(bindings.data(), bindings.size(), "Caliper");
#else
    Log(2).stream() << "mpiwrap: Using PMPI wrappers." << std::endl;
#endif
}

}

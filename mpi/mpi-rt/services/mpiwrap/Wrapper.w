#include "MpiEvents.h"

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/StringConverter.h"
#include "caliper/common/Variant.h"

#include <mpi.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>

namespace cali
{
    extern Attribute   mpifn_attr;
    extern Attribute   mpirank_attr;
    extern Attribute   mpisize_attr;
}

using namespace cali;
using namespace std;

namespace 
{

bool enable_wrapper = false;

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

// Wrap all MPI functions
{{fnall func MPI_Init MPI_Init_thread MPI_Finalize}}{
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


#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"

#include "caliper/common/util/split.hpp"

#include <mpi.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>
#include <sstream>

namespace cali
{
    extern Attribute   mpifn_attr;
    extern Attribute   mpirank_attr;
    extern Attribute   mpisize_attr;

    extern bool        mpi_enabled;
    extern bool        mpit_enabled;

    extern std::string mpi_whitelist_string;
    extern std::string mpi_blacklist_string;
	
	extern void mpit_allocate_pvar_handles();
	extern void mpit_allocate_bound_pvar_handles(void *handle, int bind);

}

using namespace cali;
using namespace std;

namespace 
{
    {{forallfn foo}}
    bool enable_{{foo}} = true;
    {{endforallfn}}

    void setup_filter() {
        std::vector<std::string> whitelist;
        std::vector<std::string> blacklist;

        util::split(mpi_whitelist_string, ':', std::back_inserter(whitelist));
        util::split(mpi_blacklist_string, ':', std::back_inserter(blacklist));

        bool have_whitelist = whitelist.size() > 0;
        bool have_blacklist = blacklist.size() > 0;

        if (!have_whitelist && !have_blacklist)
            return;

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
            std::string fnstr(e->name);

            if (have_whitelist) {
                vector<string>::iterator it = std::find(whitelist.begin(), whitelist.end(), fnstr);

                if (it != whitelist.end())
                    whitelist.erase(it);
                else
                    *(e->enableptr) = false;
            }
            if (have_blacklist) {
                vector<string>::iterator it = std::find(blacklist.begin(), blacklist.end(), fnstr);

                if (it != blacklist.end()) {
                    blacklist.erase(it);
                    *(e->enableptr) = false;
                }
            }
        }

        for (vector<string>::const_iterator it = whitelist.begin(); it != whitelist.end(); ++it)
            Log(1).stream() << "Unknown MPI function " << *it << " in MPI function whitelist" << endl;
        for (vector<string>::const_iterator it = blacklist.begin(); it != blacklist.end(); ++it)
            Log(1).stream() << "Unknown MPI function " << *it << " in MPI function blacklist" << endl;
    }
}

{{fn func MPI_Init MPI_Init_thread}}{
    {{callfn}}

    int rank = 0;
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Disable (most) logging on non-0 ranks by default
    std::ostringstream logprefix;
    logprefix << "(" << rank << "): ";

    Log::add_prefix(logprefix.str());

    if (rank > 0)
        Log::set_verbosity(0);

    // Make sure Caliper is initialized
    Caliper c;    

    if (mpi_enabled) {
        ::setup_filter();

        int size;
        PMPI_Comm_size(MPI_COMM_WORLD, &size);

        c.set(mpisize_attr, Variant(size));
        c.set(mpirank_attr, Variant(rank));
		
    }

	if(mpit_enabled) {
		Log(1).stream() << "Invoking PVAR allocating routine after MPI_Init." << endl;
		mpit_allocate_pvar_handles();
	}

}{{endfn}}

// Invoke pvar handle allocation routines for pvars bound to some MPI object

{{fn func MPI_Comm_create}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }

	if(mpit_enabled) {
		mpit_allocate_bound_pvar_handles({{2}}, MPI_T_BIND_MPI_COMM); 
	}

}{{endfn}}

{{fn func MPI_Errhandler_create}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }

	if(mpit_enabled) {
		mpit_allocate_bound_pvar_handles({{1}}, MPI_T_BIND_MPI_ERRHANDLER); 
	}

}{{endfn}}
// Wrap all MPI functions

{{fnall func MPI_Init MPI_Init_thread MPI_Comm_create}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper c;
        c.begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c.end(mpifn_attr);
    } else {
        {{callfn}}
    }
}{{endfnall}}

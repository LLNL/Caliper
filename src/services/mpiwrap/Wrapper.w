
#include <Caliper.h>

#include <Log.h>
#include <Variant.h>

#include <util/split.hpp>

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

    extern bool        mpi_enabled;

    extern std::string mpi_whitelist_string;
    extern std::string mpi_blacklist_string;
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
    Caliper* c = Caliper::instance();    

    if (mpi_enabled) {
        ::setup_filter();

        c->set(mpirank_attr, Variant(rank));
    }
}{{endfn}}

// Wrap all MPI functions

{{fnall func MPI_Init MPI_Init_thread}}{
    if (mpi_enabled && ::enable_{{func}}) {
        Caliper* c = Caliper::instance();
        c->begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c->end(mpifn_attr);
    } else {
        {{callfn}}
    }
}{{endfnall}}

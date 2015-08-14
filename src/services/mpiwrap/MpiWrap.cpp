///@file  MpiWrap.cpp
///@brief Caliper MPI service

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>
#include <RuntimeConfig.h>

using namespace cali;
using namespace std;

namespace cali
{
    Attribute mpifn_attr   { Attribute::invalid };
    Attribute mpirank_attr { Attribute::invalid };

    bool      mpi_enabled  { false };

    string    mpi_whitelist_string;
    string    mpi_blacklist_string;
}

namespace
{

ConfigSet        config;

ConfigSet::Entry configdata[] = {
    { "whitelist", CALI_TYPE_STRING, "", 
      "List of MPI functions to instrument", 
      "Colon-separated list of MPI functions to instrument.\n"
      "If set, only whitelisted MPI functions will be instrumented.\n"
      "By default, all MPI functions are instrumented." 
    },
    { "blacklist", CALI_TYPE_STRING, "",
      "List of MPI functions to filter",
      "Colon-separated list of functions to blacklist." 
    },
    ConfigSet::Terminator
};

void mpi_register(Caliper* c)
{
    config = RuntimeConfig::init("mpi", configdata);

    mpifn_attr   = 
        c->create_attribute("mpi.function", CALI_TYPE_STRING);
    mpirank_attr = 
        c->create_attribute("mpi.rank", CALI_TYPE_INT, 
                            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_SKIP_EVENTS);

    mpi_enabled = true;

    mpi_whitelist_string = config.get("whitelist").to_string();
    mpi_blacklist_string = config.get("blacklist").to_string();

    Log(1).stream() << "Registered MPI service" << endl;
}

} // anonymous namespace 


namespace cali 
{
    CaliperService MpiService = { "mpi", ::mpi_register };
} // namespace cali

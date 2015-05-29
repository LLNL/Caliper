///@file  MpiWrap.cpp
///@brief Caliper MPI service

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>

using namespace cali;
using namespace std;

namespace cali
{
    Attribute mpifn_attr   { Attribute::invalid };
    Attribute mpirank_attr { Attribute::invalid };

    bool      mpi_enabled  { false };
}

namespace
{

void mpi_register(Caliper* c)
{
    mpifn_attr   = 
        c->create_attribute("mpi.function", CALI_TYPE_STRING);
    mpirank_attr = 
        c->create_attribute("mpi.rank", CALI_TYPE_INT, 
                            CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_SKIP_EVENTS);

    mpi_enabled = true;

    Log(1).stream() << "Registered MPI service" << endl;
}

} // anonymous namespace 


namespace cali 
{
    CaliperService MpiService = { "mpi", { ::mpi_register } };
} // namespace cali

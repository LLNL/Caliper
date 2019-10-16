// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

using namespace cali;
using namespace std;

namespace cali
{

Attribute mpifn_attr   { Attribute::invalid };
Attribute mpirank_attr { Attribute::invalid };
Attribute mpisize_attr { Attribute::invalid };
Attribute mpicall_attr { Attribute::invalid };

extern void mpiwrap_init(Caliper* c, Channel* chn);

}

namespace
{

void mpi_register(Caliper* c, Channel* chn)
{
    if (mpifn_attr == Attribute::invalid)
        mpifn_attr   = 
            c->create_attribute("mpi.function", CALI_TYPE_STRING,
                                CALI_ATTR_NESTED);
    if (mpirank_attr == Attribute::invalid)
        mpirank_attr = 
            c->create_attribute("mpi.rank", CALI_TYPE_INT, 
                                CALI_ATTR_SCOPE_PROCESS |
                                CALI_ATTR_SKIP_EVENTS   |
                                CALI_ATTR_ASVALUE);
    if (mpisize_attr == Attribute::invalid)
        mpisize_attr = 
            c->create_attribute("mpi.world.size", CALI_TYPE_INT, 
                                CALI_ATTR_GLOBAL        |
                                CALI_ATTR_SKIP_EVENTS);

    mpiwrap_init(c, chn);

    Log(1).stream() << chn->name() << ": Registered MPI service" << std::endl;
}

} // anonymous namespace 


namespace cali 
{

CaliperService mpiwrap_service = { "mpi", ::mpi_register };

} // namespace cali

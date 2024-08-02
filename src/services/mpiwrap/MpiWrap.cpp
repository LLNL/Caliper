// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "../../services/Services.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

using namespace cali;
using namespace std;

namespace cali
{

Attribute mpifn_attr;
Attribute mpirank_attr;
Attribute mpisize_attr;
Attribute mpicall_attr;

extern void mpiwrap_init(Caliper* c, Channel* chn, cali::ConfigSet& cfg);

extern Attribute subscription_event_attr;

}

namespace
{

const char* mpi_service_spec = R"json(
{   "name": "mpi",
    "description": "MPI function wrapping and message tracing",
    "config": [
        {   "name": "blacklist",
            "description": "List of MPI functions to filter",
            "type": "string"
        },
        {   "name": "whitelist",
            "description": "List of MPI functions to instrument",
            "type": "string"
        },
        {   "name": "msg_tracing",
            "description": "List of MPI functions to instrument",
            "type": "bool",
            "value": "false"
        }
    ]
}
)json";

void mpi_register(Caliper* c, Channel* chn)
{
    Variant v_true(true);

    if (!mpifn_attr)
        mpifn_attr   =
            c->create_attribute("mpi.function", CALI_TYPE_STRING,
                                CALI_ATTR_NESTED,
                                1, &subscription_event_attr, &v_true);
    if (!mpirank_attr)
        mpirank_attr =
            c->create_attribute("mpi.rank", CALI_TYPE_INT,
                                CALI_ATTR_SCOPE_PROCESS |
                                CALI_ATTR_SKIP_EVENTS   |
                                CALI_ATTR_ASVALUE);
    if (!mpisize_attr)
        mpisize_attr =
            c->create_attribute("mpi.world.size", CALI_TYPE_INT,
                                CALI_ATTR_GLOBAL        |
                                CALI_ATTR_SKIP_EVENTS);

    ConfigSet cfg = services::init_config_from_spec(chn->config(), mpi_service_spec);

    mpiwrap_init(c, chn, cfg);

    Log(1).stream() << chn->name() << ": Registered MPI service" << std::endl;
}

} // anonymous namespace


namespace cali
{

CaliperService mpi_service = { ::mpi_service_spec, ::mpi_register };

} // namespace cali

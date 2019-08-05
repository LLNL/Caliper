// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include <unistd.h>
#include <sys/syscall.h>

using namespace cali;

namespace
{

Attribute cpu_attr;
Attribute node_attr;

void snapshot_cb(Caliper*, Channel*, int scopes, const SnapshotRecord*, SnapshotRecord* rec)
{
#ifdef SYS_getcpu
    if (scopes & CALI_SCOPE_THREAD) {
        unsigned cpu = 0, node = 0;

        if (syscall(SYS_getcpu, &cpu, &node, NULL) == 0) {
            rec->append(cpu_attr.id(),
                        cali_make_variant_from_uint(cpu));
            rec->append(node_attr.id(),
                        cali_make_variant_from_uint(node));
        }
    }
#endif
}

void cpuinfo_register(Caliper* c, Channel* chn)
{
    cpu_attr =
        c->create_attribute("cpuinfo.cpu",  CALI_TYPE_UINT,
                            CALI_ATTR_ASVALUE      |
                            CALI_ATTR_SKIP_EVENTS  |
                            CALI_ATTR_SCOPE_THREAD);
    node_attr =
        c->create_attribute("cpuinfo.numa_node", CALI_TYPE_UINT,
                            CALI_ATTR_ASVALUE      |
                            CALI_ATTR_SKIP_EVENTS  |
                            CALI_ATTR_SCOPE_THREAD);

    chn->events().snapshot.connect(snapshot_cb);

    Log(1).stream() << chn->name() << ": Registered cpuinfo service" << std::endl;
}

} // namespace [anonymous]


namespace cali
{

CaliperService cpuinfo_service = { "cpuinfo", ::cpuinfo_register };

}

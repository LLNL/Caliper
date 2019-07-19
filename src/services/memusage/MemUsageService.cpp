// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include <malloc.h>

using namespace cali;

namespace
{

Attribute malloc_total_bytes_attr;

void snapshot_cb(Caliper*, Channel*, int scopes, const SnapshotRecord*, SnapshotRecord* rec)
{
    if (scopes & CALI_SCOPE_PROCESS) {
        struct mallinfo mi = mallinfo();

        rec->append(malloc_total_bytes_attr.id(),
                    cali_make_variant_from_uint(mi.hblkhd + mi.uordblks));
    }
}

void memusage_register(Caliper* c, Channel* chn)
{
    Attribute aggr_attr = c->get_attribute("class.aggregatable");
    Variant v_true(true);
    
    malloc_total_bytes_attr =
        c->create_attribute("malloc.total.bytes", CALI_TYPE_UINT,
                            CALI_ATTR_SCOPE_PROCESS |
                            CALI_ATTR_ASVALUE,
                            1, &aggr_attr, &v_true);

    chn->events().snapshot.connect(snapshot_cb);

    Log(1).stream() << chn->name() << ": Registered memusage service" << std::endl;
}

}

namespace cali
{

CaliperService memusage_service = { "memusage", ::memusage_register };

}

// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Initialization of API attributes and static variables

#include "caliper/Caliper.h"

#include "caliper/common/Attribute.h"

#include "caliper/common/cali_types.h"

cali_id_t cali_class_aggregatable_attr_id  = CALI_INV_ID;
cali_id_t cali_class_symboladdress_attr_id = CALI_INV_ID;
cali_id_t cali_class_memoryaddress_attr_id = CALI_INV_ID;
cali_id_t cali_class_iteration_attr_id     = CALI_INV_ID;
cali_id_t cali_subscription_event_attr_id  = CALI_INV_ID;

cali_id_t cali_loop_attr_id   = CALI_INV_ID;
cali_id_t cali_region_attr_id = CALI_INV_ID;

cali_id_t cali_alloc_fn_attr_id                 = CALI_INV_ID;
cali_id_t cali_alloc_label_attr_id              = CALI_INV_ID;
cali_id_t cali_alloc_uid_attr_id                = CALI_INV_ID;
cali_id_t cali_alloc_addr_attr_id               = CALI_INV_ID;
cali_id_t cali_alloc_elem_size_attr_id          = CALI_INV_ID;
cali_id_t cali_alloc_num_elems_attr_id          = CALI_INV_ID;
cali_id_t cali_alloc_total_size_attr_id         = CALI_INV_ID;
cali_id_t cali_alloc_same_size_count_attr_id    = CALI_INV_ID;

namespace cali
{
    Attribute class_aggregatable_attr;
    Attribute class_symboladdress_attr;
    Attribute class_memoryaddress_attr;
    Attribute class_iteration_attr;
    Attribute subscription_event_attr;

    Attribute region_attr;
    Attribute loop_attr;

    void init_attribute_classes(Caliper* c) {
        class_aggregatable_attr =
            c->create_attribute("class.aggregatable", CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS);
        class_symboladdress_attr =
            c->create_attribute("class.symboladdress", CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS);
        class_memoryaddress_attr =
            c->create_attribute("class.memoryaddress", CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS);
        class_iteration_attr =
            c->create_attribute("class.iteration", CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS);
        subscription_event_attr =
            c->create_attribute("subscription_event", CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS);

        cali_class_aggregatable_attr_id  = class_aggregatable_attr.id();
        cali_class_symboladdress_attr_id = class_symboladdress_attr.id();
        cali_class_memoryaddress_attr_id = class_memoryaddress_attr.id();
        cali_class_iteration_attr_id     = class_iteration_attr.id();
    }

    void init_api_attributes(Caliper* c) {
        loop_attr =
            c->create_attribute("loop", CALI_TYPE_STRING, CALI_ATTR_NESTED);
        region_attr =
            c->create_attribute("region", CALI_TYPE_STRING, CALI_ATTR_NESTED);

        cali_region_attr_id = region_attr.id();
        cali_loop_attr_id = loop_attr.id();
    }
}

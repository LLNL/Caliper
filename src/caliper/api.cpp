// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
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

cali_id_t cali_function_attr_id     = CALI_INV_ID;
cali_id_t cali_loop_attr_id         = CALI_INV_ID;
cali_id_t cali_statement_attr_id    = CALI_INV_ID;
cali_id_t cali_annotation_attr_id   = CALI_INV_ID;

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
    
    Attribute function_attr;
    Attribute loop_attr;
    Attribute statement_attr;
    Attribute annotation_attr;

    void init_attribute_classes(Caliper* c) {
        struct attr_info_t {
            const char*    name;
            cali_attr_type type;
            int            prop;
            Attribute*     attr;
            cali_id_t*     attr_id;
        } attr_info[] = {
            { "class.aggregatable", CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS,
              &class_aggregatable_attr, &cali_class_aggregatable_attr_id
            },
            { "class.symboladdress", CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS,
              &class_symboladdress_attr, &cali_class_symboladdress_attr_id
            },
            { "class.memoryaddress", CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS,
              &class_memoryaddress_attr, &cali_class_memoryaddress_attr_id
            },
            { "class.iteration",     CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS,
              &class_iteration_attr, &cali_class_iteration_attr_id
            },
            { "subscription_event",  CALI_TYPE_BOOL, CALI_ATTR_SKIP_EVENTS,
              &subscription_event_attr, &cali_subscription_event_attr_id
            },
            { 0, CALI_TYPE_INV, CALI_ATTR_DEFAULT, 0, 0 }
        };

        for (attr_info_t *p = attr_info; p->name; ++p) {
            *(p->attr) =
                c->create_attribute(p->name, p->type, p->prop);
            *(p->attr_id) = (p->attr)->id();
        }
    }

    void init_api_attributes(Caliper* c) {
        // --- code annotation attributes

        struct attr_info_t {
            const char*    name;
            cali_attr_type type;
            int            prop;
            Attribute*     attr;
            cali_id_t*     attr_id;
        } attr_info[] = {
            { "function",   CALI_TYPE_STRING, CALI_ATTR_NESTED,
              &function_attr,   &cali_function_attr_id
            },
            { "loop",       CALI_TYPE_STRING, CALI_ATTR_NESTED,
              &loop_attr,       &cali_loop_attr_id
            },
            { "statement",  CALI_TYPE_STRING, CALI_ATTR_NESTED,
              &statement_attr,  &cali_statement_attr_id
            },
            { "annotation", CALI_TYPE_STRING, CALI_ATTR_NESTED,
              &annotation_attr, &cali_annotation_attr_id
            },
            { 0, CALI_TYPE_INV, CALI_ATTR_DEFAULT, 0, 0 }
        };

        Variant v_true(true);
        
        for (attr_info_t *p = attr_info; p->name; ++p) {
            *(p->attr)    = c->create_attribute(p->name, p->type, p->prop);
            *(p->attr_id) = (p->attr)->id();
        }
    }
}

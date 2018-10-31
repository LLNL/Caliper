// Copyright (c) 2016, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Initialization of API attributes and static variables

#include "caliper/Caliper.h"

#include "caliper/common/Attribute.h"

#include "caliper/common/cali_types.h"

cali_id_t cali_class_aggregatable_attr_id  = CALI_INV_ID;
cali_id_t cali_class_symboladdress_attr_id = CALI_INV_ID;
cali_id_t cali_class_memoryaddress_attr_id = CALI_INV_ID;
cali_id_t cali_class_iteration_attr_id     = CALI_INV_ID;

cali_id_t cali_function_attr_id     = CALI_INV_ID;
cali_id_t cali_loop_attr_id         = CALI_INV_ID;
cali_id_t cali_statement_attr_id    = CALI_INV_ID;
cali_id_t cali_annotation_attr_id   = CALI_INV_ID;

cali_id_t cali_alloc_fn_attr_id                 = CALI_INV_ID;
cali_id_t cali_alloc_label_attr_id              = CALI_INV_ID;
cali_id_t cali_alloc_uid_attr_id              = CALI_INV_ID;
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

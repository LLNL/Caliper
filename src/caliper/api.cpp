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

/// \file api.cpp
/// Initialization of API attributes and static variables

#include "Attribute.h"
#include "Caliper.h"

#include "cali_types.h"

namespace cali
{
    Attribute ann_type_code_attr;
    
    Attribute ann_function_attr;
    Attribute ann_loop_attr;

    void init_api_attributes(Caliper* c) {
        // --- attributes w/o metadata
        
        struct attr_info_t {
            const char*    name;
            cali_attr_type type;
            int            prop;
            Attribute*     attr;
        } attr_info[] = {
            { "annotation.type.code", CALI_TYPE_BOOL,   CALI_ATTR_SKIP_EVENTS,
              &ann_type_code_attr
            },
            { 0, CALI_TYPE_INV, CALI_ATTR_DEFAULT, 0 }
        };

        for (attr_info_t *p = attr_info; p->name; ++p)
            *(p->attr) =
                c->create_attribute(p->name, p->type, p->prop);

        // --- code annotation attributes

        attr_info_t codeattr_info[] = {
            { "annotation.function",  CALI_TYPE_STRING, CALI_ATTR_DEFAULT,
              &ann_function_attr
            },
            { "annotation.loop",      CALI_TYPE_STRING, CALI_ATTR_DEFAULT,
              &ann_loop_attr
            },
            { 0, CALI_TYPE_INV, CALI_ATTR_DEFAULT, 0 }
        };

        Variant v_true(true);
        
        for (attr_info_t *p = codeattr_info; p->name; ++p)
            *(p->attr) =
                c->create_attribute(p->name, p->type, p->prop,
                                    1, &ann_type_code_attr, &v_true);
    }
}

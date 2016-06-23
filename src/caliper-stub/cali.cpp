// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
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

/// \file cali.cpp
/// Caliper C interface stub implementation

#include <cali.h>

//
// --- Attribute interface
//

cali_id_t 
cali_create_attribute(const char* name, cali_attr_type type, int properties)
{
    return CALI_INV_ID;
}

cali_id_t
cali_create_attribute_with_metadata(const char* name, cali_attr_type type, int properties,
                                    int n,
                                    const cali_id_t meta_attr_list[],
                                    const void* meta_val_list[],
                                    const size_t meta_size_list[])
{
    return CALI_INV_ID;
}

cali_id_t
cali_find_attribute(const char* name)
{
    return CALI_INV_ID;
}


//
// --- Context interface
//

void
cali_push_snapshot(int scope, int n,
                   const cali_id_t trigger_info_attr_list[],
                   const void* trigger_info_val_list[],
                   const size_t trigger_info_size_list[])
{
}

//
// --- Annotationc interface
//

cali_err
cali_begin(cali_id_t)
{
    return CALI_SUCCESS;
}

cali_err
cali_end(cali_id_t)
{
    return CALI_SUCCESS;
}

cali_err  
cali_set(cali_id_t, const void*, size_t)
{
    return CALI_SUCCESS;
}

cali_err  
cali_begin_double(cali_id_t, double)
{
    return CALI_SUCCESS;
}

cali_err  
cali_begin_int(cali_id_t, int)
{
    return CALI_SUCCESS;
}

cali_err  
cali_begin_string(cali_id_t, const char*)
{
    return CALI_SUCCESS;
}

cali_err  
cali_set_double(cali_id_t, double)
{
    return CALI_SUCCESS;
}

cali_err  
cali_set_int(cali_id_t, int)
{
    return CALI_SUCCESS;
}

cali_err  
cali_set_string(cali_id_t, const char*)
{
    return CALI_SUCCESS;
}

cali_err
cali_begin_double_byname(const char*, double)
{
    return CALI_SUCCESS;
}

cali_err
cali_begin_int_byname(const char*, int)
{
    return CALI_SUCCESS;
}

cali_err
cali_begin_byname(const char*)
{
    return CALI_SUCCESS;
}

cali_err
cali_begin_string_byname(const char*, const char*)
{
    return CALI_SUCCESS;
}

cali_err
cali_set_double_byname(const char*, double)
{
    return CALI_SUCCESS;
}

cali_err
cali_set_int_byname(const char*, int)
{
    return CALI_SUCCESS;
}

cali_err
cali_set_string_byname(const char*, const char*)
{
    return CALI_SUCCESS;
}

cali_err
cali_end_byname(const char* )
{
    return CALI_SUCCESS;
}

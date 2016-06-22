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
/// Caliper C interface implementation

#include "cali.h"

#include "Caliper.h"
#include "EntryList.h"

#include <Variant.h>

#include <cstring>
#include <unordered_map>
#include <mutex>


using namespace cali;


namespace
{
    // Put attributes into a local map until we get fast by-id lookup in the runtime again

    typedef std::unordered_map<cali_id_t, Attribute> attr_map_t;

    attr_map_t attr_map;
    std::mutex attr_map_mutex;

    void 
    store_attribute(const Attribute& attr) {
        if (!(attr == Attribute::invalid)) {
            std::lock_guard<std::mutex> lock(attr_map_mutex);
            attr_map.insert(std::make_pair(attr.id(), attr));;
        }
    }

    Attribute 
    lookup_attribute(Caliper& c, cali_id_t attr_id) {
        Attribute attr { Attribute::invalid };

        std::lock_guard<std::mutex> lock(attr_map_mutex);
        auto f = attr_map.find(attr_id);

        if (f != attr_map.end()) { 
            attr = f->second;
        } else {
            attr = c.get_attribute(attr_id);

            if (!(attr == Attribute::invalid))
                attr_map.insert(std::make_pair(attr_id, attr));
        }

        return attr;
    }
}


//
// --- Attribute interface
//

cali_id_t 
cali_create_attribute(const char* name, cali_attr_type type, int properties)
{
    Attribute a = Caliper::instance().create_attribute(name, type, properties);
    ::store_attribute(a);

    return a.id();
}

cali_id_t
cali_create_attribute_with_metadata(const char* name, cali_attr_type type, int properties,
                                    int n,
                                    const cali_id_t meta_attr_list[],
                                    const void* meta_val_list[],
                                    const size_t meta_size_list[])
{
    if (n < 1)
        return cali_create_attribute(name, type, properties);

    Caliper    c;

    Attribute* meta_attr = new Attribute[n];
    Variant*   meta_data = new Variant[n];

    for (int i = 0; i < n; ++i) {
        meta_attr[i] = ::lookup_attribute(c, meta_attr_list[i]);

        if (meta_attr[i] == Attribute::invalid)
            continue;

        meta_data[i] = Variant(meta_attr[i].type(), meta_val_list[i], meta_size_list[i]);
    }

    Attribute attr =
        c.create_attribute(name, type, properties, n, meta_attr, meta_data);
    
    ::store_attribute(attr);

    delete[] meta_data;
    delete[] meta_attr;

    return attr.id();
}

cali_id_t
cali_find_attribute(const char* name)
{
    Attribute a = Caliper::instance().get_attribute(name);
    ::store_attribute(a);

    return a.id();
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
    Caliper   c;
    
    Attribute attr[64];
    Variant   data[64];
        
    for (int i = 0; i < n; ++i) {
        attr[i] = ::lookup_attribute(c, trigger_info_attr_list[i]);
        data[i]  = Variant(attr[i].type(), trigger_info_val_list[i], trigger_info_size_list[i]);
    }

    EntryList::FixedEntryList<64> trigger_info_data;
    EntryList trigger_info(trigger_info_data);

    c.make_entrylist(n, attr, data, trigger_info);
    c.push_snapshot(scope, &trigger_info);
}

//
// --- Annotation interface
//

cali_err
cali_begin(cali_id_t attr_id)
{
    Caliper   c;
    Attribute attr = ::lookup_attribute(c, attr_id);
    
    if (attr.type() == CALI_TYPE_BOOL)
        return c.begin(attr, Variant(true));
    else
        return CALI_ETYPE;
}

cali_err
cali_end(cali_id_t attr_id)
{
    Caliper   c;
    Attribute attr = ::lookup_attribute(c, attr_id);

    return c.end(attr);
}

cali_err  
cali_set(cali_id_t attr_id, const void* value, size_t size)
{
    Caliper   c;
    Attribute attr = ::lookup_attribute(c, attr_id);

    return c.set(attr, Variant(attr.type(), value, size));
}

cali_err
cali_begin_double(cali_id_t attr_id, double val)
{
    Caliper   c;
    Attribute attr = ::lookup_attribute(c, attr_id);

    if (attr.type() != CALI_TYPE_DOUBLE)
        return CALI_ETYPE;

    return c.begin(attr, Variant(val));
}

cali_err
cali_begin_int(cali_id_t attr_id, int val)
{
    Caliper   c;
    Attribute attr = ::lookup_attribute(c, attr_id);

    if (attr.type() != CALI_TYPE_INT)
        return CALI_ETYPE;

    return c.begin(attr, Variant(val));
}

cali_err
cali_begin_string(cali_id_t attr_id, const char* val)
{
    Caliper   c;
    Attribute attr = ::lookup_attribute(c, attr_id);

    if (attr.type() != CALI_TYPE_STRING)
        return CALI_ETYPE;

    return c.begin(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

cali_err
cali_set_double(cali_id_t attr_id, double val)
{
    Caliper   c;
    Attribute attr = ::lookup_attribute(c, attr_id);

    if (attr.type() != CALI_TYPE_DOUBLE)
        return CALI_ETYPE;

    return c.set(attr, Variant(val));
}

cali_err
cali_set_int(cali_id_t attr_id, int val)
{
    Caliper   c;
    Attribute attr = ::lookup_attribute(c, attr_id);

    if (attr.type() != CALI_TYPE_INT)
        return CALI_ETYPE;

    return c.set(attr, Variant(val));
}

cali_err
cali_set_string(cali_id_t attr_id, const char* val)
{
    Caliper   c;
    Attribute attr = ::lookup_attribute(c, attr_id);

    if (attr.type() != CALI_TYPE_STRING)
        return CALI_ETYPE;

    return c.set(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

//
// --- By-name annotation interface 
//

cali_err
cali_begin_byname(const char* attr_name)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(attr_name, CALI_TYPE_BOOL, CALI_ATTR_DEFAULT);

    if (attr == Attribute::invalid)
        return CALI_EINV;
    if (attr.type() != CALI_TYPE_BOOL)
        return CALI_ETYPE;

    return c.begin(attr, Variant(true));
}

cali_err
cali_begin_double_byname(const char* attr_name, double val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(attr_name, CALI_TYPE_DOUBLE, CALI_ATTR_DEFAULT);

    if (attr == Attribute::invalid || attr.type() != CALI_TYPE_DOUBLE)
        return CALI_EINV;

    return c.begin(attr, Variant(val));
}

cali_err
cali_begin_int_byname(const char* attr_name, int val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(attr_name, CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    if (attr == Attribute::invalid || attr.type() != CALI_TYPE_INT)
        return CALI_EINV;

    return c.begin(attr, Variant(val));
}

cali_err
cali_begin_string_byname(const char* attr_name, const char* val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(attr_name, CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

    if (attr == Attribute::invalid || attr.type() != CALI_TYPE_STRING)
        return CALI_EINV;

    return c.begin(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

cali_err
cali_set_double_byname(const char* attr_name, double val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(attr_name, CALI_TYPE_DOUBLE, CALI_ATTR_DEFAULT);

    if (attr == Attribute::invalid || attr.type() != CALI_TYPE_DOUBLE)
        return CALI_EINV;

    return c.set(attr, Variant(val));
}

cali_err
cali_set_int_byname(const char* attr_name, int val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(attr_name, CALI_TYPE_INT, CALI_ATTR_DEFAULT);

    if (attr == Attribute::invalid || attr.type() != CALI_TYPE_INT)
        return CALI_EINV;

    return c.set(attr, Variant(val));
}

cali_err
cali_set_string_byname(const char* attr_name, const char* val)
{
    Caliper   c;
    Attribute attr =
        c.create_attribute(attr_name, CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

    if (attr == Attribute::invalid || attr.type() != CALI_TYPE_STRING)
        return CALI_EINV;

    return c.set(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

cali_err
cali_end_byname(const char* attr_name)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_name);

    return c.end(attr);
}

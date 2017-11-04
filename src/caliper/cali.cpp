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

#include "caliper/cali.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/CompressedSnapshotRecord.h"
#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Variant.h"

#include <cstring>
#include <unordered_map>
#include <mutex>


using namespace cali;

//
// --- Attribute interface
//

cali_id_t 
cali_create_attribute(const char* name, cali_attr_type type, int properties)
{
    Attribute a = Caliper::instance().create_attribute(name, type, properties);

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
        meta_attr[i] = c.get_attribute(meta_attr_list[i]);

        if (meta_attr[i] == Attribute::invalid)
            continue;

        meta_data[i] = Variant(meta_attr[i].type(), meta_val_list[i], meta_size_list[i]);
    }

    Attribute attr =
        c.create_attribute(name, type, properties, n, meta_attr, meta_data);

    delete[] meta_data;
    delete[] meta_attr;

    return attr.id();
}

cali_id_t
cali_find_attribute(const char* name)
{
    Attribute a = Caliper::instance().get_attribute(name);

    return a.id();
}

cali_attr_type
cali_attribute_type(cali_id_t attr_id)
{
    Attribute a = Caliper::instance().get_attribute(attr_id);
    return a.type();
}

const char*
cali_attribute_name(cali_id_t attr_id)
{
    return Caliper::instance().get_attribute(attr_id).name_c_str();
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

    n = std::min(std::max(n, 0), 64);

    for (int i = 0; i < n; ++i) {
        attr[i] = c.get_attribute(trigger_info_attr_list[i]);
        data[i]  = Variant(attr[i].type(), trigger_info_val_list[i], trigger_info_size_list[i]);
    }

    SnapshotRecord::FixedSnapshotRecord<64> trigger_info_data;
    SnapshotRecord trigger_info(trigger_info_data);

    c.make_entrylist(n, attr, data, trigger_info);
    c.push_snapshot(scope, &trigger_info);
}

size_t
cali_pull_snapshot(int scopes, size_t len, unsigned char* buf)
{
    Caliper c = Caliper::sigsafe_instance();

    if (!c)
        return 0;

    SnapshotRecord::FixedSnapshotRecord<80> snapshot_buffer;
    SnapshotRecord snapshot(snapshot_buffer);

    c.pull_snapshot(scopes, nullptr, &snapshot);

    CompressedSnapshotRecord rec(len, buf);
    rec.append(&snapshot);

    return rec.needed_len();
}

//
// --- Snapshot parsing
//

namespace
{
    // Helper operator to unpack entries from
    // CompressedSnapshotRecordView::unpack()
    
    class UnpackEntryOp {
        void*              m_arg;
        cali_entry_proc_fn m_fn;

    public:

        UnpackEntryOp(cali_entry_proc_fn fn, void* user_arg)
            : m_arg(user_arg), m_fn(fn)
        { }

        inline bool operator()(const Entry& e) {
            if (e.is_immediate()) {
                if ((*m_fn)(m_arg, e.attribute(), e.value().c_variant()) == 0)
                    return false;
            } else {
                for (const Node* node = e.node(); node && node->id() != CALI_INV_ID; node = node->parent())
                    if ((*m_fn)(m_arg, node->attribute(), node->data().c_variant()) == 0)
                        return false;
            }

            return true;
        }
    };

    class UnpackAttributeEntryOp {
        void*              m_arg;
        cali_entry_proc_fn m_fn;
        cali_id_t          m_id;

    public:

        UnpackAttributeEntryOp(cali_id_t id, cali_entry_proc_fn fn, void* user_arg)
            : m_arg(user_arg), m_fn(fn), m_id(id)
        { }

        inline bool operator()(const Entry& e) {            
            if (e.is_immediate() && e.attribute() == m_id) {
                if ((*m_fn)(m_arg, e.attribute(), e.value().c_variant()) == 0)
                    return false;
            } else {
                for (const Node* node = e.node(); node && node->id() != CALI_INV_ID; node = node->parent())
                    if (node->attribute() == m_id)
                        if ((*m_fn)(m_arg, node->attribute(), node->data().c_variant()) == 0)
                            return false;
            }

            return true;
        }        
    };
}

void
cali_unpack_snapshot(const unsigned char* buf,
                     size_t*              bytes_read,
                     cali_entry_proc_fn   proc_fn,
                     void*                user_arg)
{
    size_t pos = 0;    
    ::UnpackEntryOp op(proc_fn, user_arg);

    // FIXME: Need sigsafe instance? Only does read-only
    // node-by-id lookup though, so we should be safe
    Caliper c;

    CompressedSnapshotRecordView(buf, &pos).unpack(&c, op);

    if (bytes_read)
        *bytes_read += pos;
}

cali_variant_t
cali_find_first_in_snapshot(const unsigned char* buf,
                            cali_id_t            attr_id,
                            size_t*              bytes_read)
{
    size_t  pos = 0;
    Variant res;

    Caliper c;

    CompressedSnapshotRecordView(buf, &pos).unpack(&c, [&res,attr_id](const Entry& e) {
            if (e.is_immediate()) {
                if (e.attribute() == attr_id) {
                    res = e.value();
                    return false;
                }
            } else {
                for (const Node* node = e.node(); node; node = node->parent())
                    if (node->attribute() == attr_id) {
                        res = node->data();
                        return false;
                    }
            }

            return true;
        });

    if (bytes_read)
        *bytes_read += pos;

    return res.c_variant();
}

void
cali_find_all_in_snapshot(const unsigned char* buf,
                          cali_id_t            attr_id,
                          size_t*              bytes_read,
                          cali_entry_proc_fn   proc_fn,
                          void*                user_arg)
{
    size_t pos = 0;    
    ::UnpackAttributeEntryOp op(attr_id, proc_fn, user_arg);

    // FIXME: Need sigsafe instance? Only does read-only
    // node-by-id lookup though, so we should be safe
    Caliper c;

    CompressedSnapshotRecordView(buf, &pos).unpack(&c, op);

    if (bytes_read)
        *bytes_read += pos;
}

//
// --- Blackboard access interface
//

cali_variant_t
cali_get(cali_id_t attr_id)
{
    Caliper c = Caliper::sigsafe_instance();

    if (!c)
        return cali_make_empty_variant();

    return c.get(c.get_attribute(attr_id)).value().c_variant();
}

//
// --- Annotation interface
//

cali_err
cali_begin(cali_id_t attr_id)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);
    
    if (attr.type() == CALI_TYPE_BOOL)
        return c.begin(attr, Variant(true));
    else
        return CALI_ETYPE;
}

cali_err
cali_end(cali_id_t attr_id)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    return c.end(attr);
}

cali_err  
cali_set(cali_id_t attr_id, const void* value, size_t size)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    return c.set(attr, Variant(attr.type(), value, size));
}

cali_err
cali_begin_double(cali_id_t attr_id, double val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    if (attr.type() != CALI_TYPE_DOUBLE)
        return CALI_ETYPE;

    return c.begin(attr, Variant(val));
}

cali_err
cali_begin_int(cali_id_t attr_id, int val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    if (attr.type() != CALI_TYPE_INT)
        return CALI_ETYPE;

    return c.begin(attr, Variant(val));
}

cali_err
cali_begin_string(cali_id_t attr_id, const char* val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    if (attr.type() != CALI_TYPE_STRING)
        return CALI_ETYPE;

    return c.begin(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

cali_err
cali_set_double(cali_id_t attr_id, double val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    if (attr.type() != CALI_TYPE_DOUBLE)
        return CALI_ETYPE;

    return c.set(attr, Variant(val));
}

cali_err
cali_set_int(cali_id_t attr_id, int val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    if (attr.type() != CALI_TYPE_INT)
        return CALI_ETYPE;

    return c.set(attr, Variant(val));
}

cali_err
cali_set_string(cali_id_t attr_id, const char* val)
{
    Caliper   c;
    Attribute attr = c.get_attribute(attr_id);

    if (attr.type() != CALI_TYPE_STRING)
        return CALI_ETYPE;

    return c.set(attr, Variant(CALI_TYPE_STRING, val, strlen(val)));
}

cali_err
cali_safe_end_string(cali_id_t attr_id, const char* val)
{
    cali_err  ret  = CALI_SUCCESS;

    Caliper   c;

    Attribute attr = c.get_attribute(attr_id);
    Variant   v    = c.get(attr).value();

    if (attr.type() != CALI_TYPE_STRING || v.type() != CALI_TYPE_STRING)
        ret = CALI_ETYPE;

    if (0 != strncmp(static_cast<const char*>(v.data()), val, v.size())) {
        // FIXME: Replace log output with smart error tracker
        Log(1).stream() << "begin/end marker mismatch: Trying to end " 
                        << attr.name() << "=" << val
                        << " but current value for " 
                        << attr.name() << " is \"" << v.to_string() << "\""
                        << std::endl;
    }

    c.end(attr);

    return ret;    
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

void
cali_config_preset(const char* key, const char* value)
{
    RuntimeConfig::preset(key, value);
}

void
cali_config_set(const char* key, const char* value)
{
    RuntimeConfig::set(key, value);
}

void
cali_config_define_profile(const char* name, const char* keyvallist[][2])
{
    RuntimeConfig::define_profile(name, keyvallist);
}

void
cali_config_allow_read_env(int allow)
{
    RuntimeConfig::allow_read_env(allow != 0);
}

void
cali_init()
{
    Caliper::instance();
}

int
cali_is_initialized()
{
    return Caliper::is_initialized() ? 1 : 0;
}

//
// --- Helper functions for high-level macro interface
// 

cali_id_t
cali_make_loop_iteration_attribute(const char* name)
{
    char tmp[80] = "iteration#";
    strncpy(tmp+10, name, 69);
    tmp[79] = '\0';

    return cali_create_attribute(tmp, CALI_TYPE_INT, CALI_ATTR_ASVALUE);
}

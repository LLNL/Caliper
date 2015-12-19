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

/// @file Attribute.cpp
/// Attribute class implementation

#include "Attribute.h"

#include "Node.h"

using namespace cali;
using namespace std;

/*
namespace 
{
    int read_properties(const std::string& str)
    {
        const map<string, cali_attr_properties> propmap = { 
            { "value",         CALI_ATTR_ASVALUE       }, 
            { "nomerge",       CALI_ATTR_NOMERGE       }, 
            { "scope_process", CALI_ATTR_SCOPE_PROCESS },
            { "scope_thread",  CALI_ATTR_SCOPE_THREAD  },
            { "scope_task",    CALI_ATTR_SCOPE_TASK    } };

        int prop = CALI_ATTR_DEFAULT;

        vector<string> props;
        util::split(str, ':', back_inserter(props));

        for (const string &s : props) { 
            auto it = propmap.find(s);

            if (it != propmap.end())
                prop |= it->second;
        }

        return prop;
    }

    string write_properties(int properties) 
    {
        const struct property_tbl_entry {
            cali_attr_properties p; int mask; const char* str;
        } property_tbl[] = { 
            { CALI_ATTR_ASVALUE,       CALI_ATTR_ASVALUE,     "value"          }, 
            { CALI_ATTR_NOMERGE,       CALI_ATTR_NOMERGE,     "nomerge"        }, 
            { CALI_ATTR_SCOPE_PROCESS, CALI_ATTR_SCOPE_MASK,  "scope_process"  },
            { CALI_ATTR_SCOPE_TASK,    CALI_ATTR_SCOPE_MASK,  "scope_task"     },
            { CALI_ATTR_SCOPE_THREAD,  CALI_ATTR_SCOPE_MASK,  "scope_thread"   },
            { CALI_ATTR_SKIP_EVENTS,   CALI_ATTR_SKIP_EVENTS, "skip_events"    },
            { CALI_ATTR_HIDDEN,        CALI_ATTR_HIDDEN,      "hidden"         }
        };

        int    count = 0;
        string str;

        for ( property_tbl_entry e : property_tbl )
            if (e.p == (e.mask & properties))
                str.append(count++ > 0 ? ":" : "").append(e.str);

        return str;
    }
}
*/

Attribute 
Attribute::make_attribute(const Node* node, const MetaAttributeIDs* keys)
{
    // sanity check: make sure we have the necessary attributes (name and type)

    // Given node must be attribute name 

    if (!node || !keys || node->attribute() == CALI_INV_ID || node->attribute() != keys->name_attr_id)
        return Attribute::invalid;

    // Find type attribute
    for (const Node* p = node; p && p->attribute() != CALI_INV_ID; p = p->parent()) 
        if (p->attribute() == keys->type_attr_id)
            return Attribute(node, keys);

    return Attribute::invalid;
}

cali_id_t
Attribute::id() const
{ 
    return m_node ? m_node->id() : CALI_INV_ID;
}

std::string
Attribute::name() const 
{
    for (const Node* node = m_node; node && node->attribute() != CALI_INV_ID; node = node->parent())
        if (node->attribute() == m_keys->name_attr_id)
            return node->data().to_string();

    return std::string();
}

cali_attr_type
Attribute::type() const 
{
    for (const Node* node = m_node; node && node->attribute() != CALI_INV_ID; node = node->parent())
        if (node->attribute() == m_keys->type_attr_id)
            return node->data().to_attr_type();

    return CALI_TYPE_INV;
}

int
Attribute::properties() const 
{
    for (const Node* node = m_node; node && node->attribute() != CALI_INV_ID; node = node->parent())
        if (node->attribute() == m_keys->prop_attr_id)
            return node->data().to_int();

    return CALI_ATTR_DEFAULT;
}

const MetaAttributeIDs MetaAttributeIDs::invalid { CALI_INV_ID, CALI_INV_ID, CALI_INV_ID };

const Attribute Attribute::invalid { 0, 0 };

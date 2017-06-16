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

#include "caliper/common/Attribute.h"

#include "caliper/common/Node.h"

using namespace cali;
using namespace std;

Attribute 
Attribute::make_attribute(const Node* node)
{
    // sanity check: make sure we have the necessary attributes (name and type)

    // Given node must be attribute name 

    if (!node || node->attribute() == CALI_INV_ID || node->attribute() != s_keys.name_attr_id)
        return Attribute::invalid;

    // Find type attribute
    for (const Node* p = node; p && p->attribute() != CALI_INV_ID; p = p->parent()) 
        if (p->attribute() == s_keys.type_attr_id)
            return Attribute(node);

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
        if (node->attribute() == s_keys.name_attr_id)
            return node->data().to_string();

    return std::string();
}

const char*
Attribute::name_c_str() const
{
    for (const Node* node = m_node; node && node->attribute() != CALI_INV_ID; node = node->parent())
        if (node->attribute() == s_keys.name_attr_id)
            return static_cast<const char*>(node->data().data());

    return nullptr;
}

cali_attr_type
Attribute::type() const 
{
    for (const Node* node = m_node; node && node->attribute() != CALI_INV_ID; node = node->parent())
        if (node->attribute() == s_keys.type_attr_id)
            return node->data().to_attr_type();

    return CALI_TYPE_INV;
}

int
Attribute::properties() const 
{
    for (const Node* node = m_node; node && node->attribute() != CALI_INV_ID; node = node->parent())
        if (node->attribute() == s_keys.prop_attr_id)
            return node->data().to_int();

    return CALI_ATTR_DEFAULT;
}

Variant
Attribute::get(const Attribute& attr) const
{
    for (const Node* node = m_node; node; node = node->parent())
        if (node->attribute() == attr.id())
            return node->data();

    return Variant();
}

std::ostream&
cali::operator << (std::ostream& os, const Attribute& a)
{
    char buf[256];

    cali_prop2string(a.properties(), buf, 256);

    return os << "{ \"id\" : " << a.id()
              << ", \"name\" : \"" << a.name() << "\""
              << ", \"type\" : \"" << cali_type2string(a.type()) << "\""
              << ", \"properties\" : \"" << buf << "\" }";
}

const MetaAttributeIDs MetaAttributeIDs::invalid { CALI_INV_ID, CALI_INV_ID, CALI_INV_ID };
const MetaAttributeIDs Attribute::s_keys { 8, 9, 10 };

const Attribute Attribute::invalid { 0 };

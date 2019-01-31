// Copyright (c) 2015-2017, Lawrence Livermore National Security, LLC.  
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

// Entry class definition

#include "caliper/common/Entry.h"
#include "caliper/common/Node.h"

#include <functional>

using namespace cali;

int 
Entry::count(cali_id_t attr_id) const 
{
    int res = 0;

    if (m_node) {
        for (const Node* node = m_node; node; node = node->parent())
            if (node->attribute() == attr_id)
                ++res;
    } else {
        if (m_attr_id != CALI_INV_ID && m_attr_id == attr_id)
            ++res;
    }

    return res;
}

Variant 
Entry::value(cali_id_t attr_id) const
{
    if (!m_node && attr_id == m_attr_id)
	return m_value;

    for (const Node* node = m_node; node; node = node->parent())
	if (node->attribute() == attr_id)
	    return node->data();

    return Variant();
}

bool cali::operator == (const Entry& lhs, const Entry& rhs)
{
    if (lhs.m_node)
        return rhs.m_node ? (lhs.m_node->id() == rhs.m_node->id()) : false;
    else if (rhs.m_node)
        return false;
    else if (lhs.m_attr_id == rhs.m_attr_id)
        return lhs.m_value == rhs.m_value;

    return false;
}
                  
const Entry Entry::empty;

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

/// @file ContextRecord.cpp
/// ContextRecord implementation

#include "ContextRecord.h"

#include "Node.h"

using namespace cali;
using namespace std;

namespace 
{
    const char* RecordElements[] = { "ref", "attr", "data" };
}

const RecordDescriptor ContextRecord::s_record { 0x101, "ctx", 3, ::RecordElements };


RecordMap
ContextRecord::unpack(const RecordMap& rec, std::function<const Node*(cali_id_t)> get_node)
{
    RecordMap out;

    auto entry_it = rec.find("__rec");

    if (entry_it == rec.end() || entry_it->second.empty() || entry_it->second.front().to_string() != "ctx")
        return out;

    // implicit entries: 

    entry_it = rec.find("ref");

    if (entry_it != rec.end())
        for (const Variant& elem : entry_it->second) {
            const Node* node = get_node(elem.to_id());

            for ( ; node && node->id() != CALI_INV_ID; node = node->parent() ) {
                const Node* attr_node = get_node(node->attribute());

                if (attr_node)
                    out[attr_node->data().to_string()].push_back(node->data());
            }
        }

    auto expl_entry_it = rec.find("attr");
    auto data_entry_it = rec.find("data");

    if (expl_entry_it == rec.end() || data_entry_it == rec.end() || expl_entry_it->second.empty())
        return out;

    for (unsigned i = 0; i < expl_entry_it->second.size() && i < data_entry_it->second.size(); ++i) {
        const Node* attr_node = get_node(expl_entry_it->second[i].to_id());

        if (attr_node)
            out[attr_node->data().to_string()].push_back(data_entry_it->second[i]);
    }

    return out;
}

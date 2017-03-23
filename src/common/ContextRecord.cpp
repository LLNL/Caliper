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

#include "CaliperMetadataAccessInterface.h"
#include "Node.h"
#include "StringConverter.h"

using namespace cali;
using namespace std;

namespace 
{
    const char* RecordElements[] = { "ref", "attr", "data" };

    inline cali_id_t
    id_from_string(const std::string& str) {
        bool ok = false;
        cali_id_t id = StringConverter(str).to_uint(&ok);

        return ok ? id : CALI_INV_ID;
    }
}

const RecordDescriptor ContextRecord::s_record { 0x101, "ctx", 3, ::RecordElements };


RecordMap
ContextRecord::unpack(const RecordMap& rec, const CaliperMetadataAccessInterface& metadb)
{
    RecordMap out;

    auto entry_it = rec.find("__rec");

    if (entry_it == rec.end() || entry_it->second.empty() || entry_it->second.front() != "ctx")
        return out;

    // implicit entries: 

    entry_it = rec.find("ref");

    if (entry_it != rec.end())
        for (const std::string& str : entry_it->second) {
            const Node* node = metadb.node(::id_from_string(str));

            for ( ; node && node->id() != CALI_INV_ID; node = node->parent() ) {
                Attribute attr = metadb.get_attribute(node->attribute());

                if (attr != Attribute::invalid)
                    out[attr.name()].push_back(node->data().to_string());
            }
        }

    auto expl_entry_it = rec.find("attr");
    auto data_entry_it = rec.find("data");

    if (expl_entry_it == rec.end() || data_entry_it == rec.end() || expl_entry_it->second.empty())
        return out;

    for (unsigned i = 0; i < expl_entry_it->second.size() && i < data_entry_it->second.size(); ++i) {
        Attribute attr = metadb.get_attribute(::id_from_string(expl_entry_it->second[i]));

        if (attr != Attribute::invalid)
            out[attr.name()].push_back(data_entry_it->second[i]);
    }

    return out;
}

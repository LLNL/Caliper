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

#include "util/split.hpp"

#include <iterator>
#include <map>
#include <vector>

using namespace cali;
using namespace std;


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

const Attribute Attribute::invalid { CALI_INV_ID, "", CALI_TYPE_INV, CALI_ATTR_DEFAULT };

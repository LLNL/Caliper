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

/// @file AttributeStore.cpp
/// AttributeStore class implementation

#include "AttributeStore.h"

#include <map>
#include <vector>

using namespace cali;
using namespace std;

struct AttributeStore::AttributeStoreImpl
{
    // --- Data
    
    vector<Attribute>      attributes;
    map<string, cali_id_t> namelist;


    Attribute create(const std::string&  name, cali_attr_type type, int properties) {
        auto it = namelist.find(name);

        if (it != namelist.end())
            return attributes[it->second];

        cali_id_t id = attributes.size();

        namelist.insert(make_pair(name, id));
        attributes.push_back(Attribute(id, name, type, properties));

        return attributes.back();
    }

    size_t size() const {
        return attributes.size();
    }

    Attribute get(cali_id_t id) const {
        if (id < attributes.size())
            return attributes[id];

        return Attribute::invalid;
    }

    Attribute get(const std::string& name) const {
        auto it = namelist.find(name);

        if (it == namelist.end())
            return Attribute::invalid;

        return attributes[it->second];
    }

    void foreach_attribute(std::function<void(const Attribute&)> proc) const {
        for ( const Attribute& a : attributes )
            proc(a);
    }

    // void read(AttributeReader& r) {
    //     attributes.clear();
    //     namelist.clear();

    //     for (AttributeReader::AttributeInfo info = r.read(); info.id != CALI_INV_ID; info = r.read()) {
    //         if (attributes.size() < info.id)
    //             attributes.reserve(info.id);

    //         attributes[info.id] = Attribute(info.id, info.name, info.type, info.properties);
    //         namelist.insert(make_pair(info.name, info.id));
    //     }
    // }
};


//
// --- AttributeStore interface
//

AttributeStore::AttributeStore()
    : mP { new AttributeStoreImpl }
{ 
}

AttributeStore::~AttributeStore()
{
    mP.reset();
}

size_t AttributeStore::size() const
{
    return mP->size();
}

Attribute AttributeStore::get(cali_id_t id) const
{
    return mP->get(id);
}

Attribute AttributeStore::get(const std::string& name) const
{
    return mP->get(name);;
}

Attribute AttributeStore::create(const std::string& name, cali_attr_type type, int properties)
{
    return mP->create(name, type, properties);
}

void AttributeStore::foreach_attribute(std::function<void(const Attribute&)> proc) const
{
    mP->foreach_attribute(proc);
}

// void AttributeStore::read(AttributeReader& r)
// {
//     mP->read(r);
// }

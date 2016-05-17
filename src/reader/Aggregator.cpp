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

/// @file Aggregator.cpp
/// Aggregator implementation

#include "Aggregator.h"

#include "CaliperMetadataDB.h"

#include <Attribute.h>
#include <Log.h>
#include <Node.h>

#include <cali_types.h>

#include <util/split.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>

using namespace cali;
using namespace std;

namespace
{
    size_t entrylist_hash(EntryList& list) {
        // normalize list by sorting
        std::sort(list.begin(), list.end());

        size_t hash = 0;

        for (const Entry& e : list)
            hash = hash ^ (e.hash() << 1);

        return hash;
    }
}

struct Aggregator::AggregatorImpl
{
    // --- data

    vector<string>    m_aggr_attribute_strings;
    vector<cali_id_t> m_aggr_attribute_ids;

    map< EntryList, EntryList >      m_aggr_db;


    // --- interface

    void parse(const string& aggr_config) {
        vector<string> attributes;

        util::split(aggr_config, ':', back_inserter(m_aggr_attribute_strings));
    }

    void update_aggr_attribute_ids(CaliperMetadataDB& db, const EntryList& list) {
        if (m_aggr_attribute_strings.empty())
            return;
        
        // see we can find one of the attributes for which we don't know numeric ID's yet

        for (const Entry& e : list)
            if (!e.node()) {
                Attribute attr = db.attribute(e.attribute());

                if (attr == Attribute::invalid)
                    continue;

                auto it = std::find(m_aggr_attribute_strings.begin(), m_aggr_attribute_strings.end(), attr.name());

                if (it != m_aggr_attribute_strings.end()) {
                    m_aggr_attribute_ids.push_back(attr.id());
                    m_aggr_attribute_strings.erase(it);
                }
            }
    }    

    Variant aggregate(const Attribute& attr, const Variant& l, const Variant& r) {
        switch (attr.type()) {
        case CALI_TYPE_INT:
            return Variant(l.to_int() + r.to_int());
        case CALI_TYPE_UINT:
            return Variant(l.to_uint() + r.to_uint());
        case CALI_TYPE_DOUBLE:
            return Variant(l.to_double() + r.to_double());
        default:
            Log(0).stream() << "Cannot aggregate attribute " << attr.name() 
                            << " with non-aggregatable type " 
                            << cali_type2string(attr.type()) 
                            << endl;
        }

        return Variant();
    }

    void process(CaliperMetadataDB& db, const EntryList& list) {
        update_aggr_attribute_ids(db, list);

        EntryList key_vec;

        for (const Entry& e : list)
            if (std::find(m_aggr_attribute_ids.begin(), m_aggr_attribute_ids.end(), e.attribute()) == m_aggr_attribute_ids.end())
                key_vec.push_back(e);

        // if there aren't any entries to aggregate, just return
        if (key_vec.size() == list.size())
            return;

        // normalize key by sorting
        std::sort(key_vec.begin(), key_vec.end());

        auto it = m_aggr_db.find(key_vec);

        if (it == m_aggr_db.end()) {
            m_aggr_db.insert(std::make_pair(key_vec, list));
        } else {
            for (cali_id_t id : m_aggr_attribute_ids) {
                auto find_entry =
                    [id](const Entry& e) { return e.attribute() == id; } ;
                
                auto elst_it = std::find_if(list.begin(), list.end(), find_entry);

                if (elst_it != list.end()) {
                    auto alst_it = std::find_if(it->second.begin(), it->second.end(), find_entry);

                    if (alst_it != it->second.end()) {
                        Attribute attr = db.attribute(id);
                        
                        *alst_it = Entry(attr, aggregate(attr, alst_it->value(), elst_it->value()));
                    }
                }
            }
        }
    }

    void flush(CaliperMetadataDB& db, const SnapshotProcessFn push) {
        for (const auto p : m_aggr_db)
            push(db, p.second);
    }
};

Aggregator::Aggregator(const string& aggr_config)
    : mP { new AggregatorImpl() }
{
    mP->parse(aggr_config);
}

Aggregator::~Aggregator()
{
    mP.reset();
}

void 
Aggregator::flush(CaliperMetadataDB& db, SnapshotProcessFn push)
{
    mP->flush(db, push);
}

void
Aggregator::operator()(CaliperMetadataDB& db, const EntryList& list)
{
    mP->process(db, list);
}

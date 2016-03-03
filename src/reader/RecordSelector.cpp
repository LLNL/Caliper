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

/// @file RecordSelector.cpp
/// RecordSelector implementation

#include "RecordSelector.h"

#include "CaliperMetadataDB.h"

#include <Attribute.h>
#include <Node.h>

#include <util/split.hpp>

#include <iostream>
#include <iterator>

using namespace cali;
using namespace std;

struct RecordSelector::RecordSelectorImpl
{
    enum class Op { Contains, Equals, Less, Greater };

    struct Clause {
        std::string attr_name;
        cali_id_t   attr_id;
        std::string value;
        Op          op;
        bool        negate;
    };

    std::vector<Clause> m_clauses;

    bool parse_clause(const string& str) {
        // parse "[-]attribute[(<>=)value]" string

        if (str.empty())
            return false;

        Clause clause { "", CALI_INV_ID, "", Op::Contains, false };
        string::size_type spos = 0;

        if (str[spos] == '-') {
            ++spos;
            clause.negate = true;
        }

        string::size_type opos = str.find_first_of("<>=", spos);

        clause.attr_name.assign(str, spos, opos < string::npos ? opos-spos : opos);

        if (opos < str.size()-1) {
            clause.value.assign(str, opos+1, string::npos);

            struct ops_t { char c; Op op; } const ops[] = {
                // { '<', Op::Less     }, { '>', Op::Greater  },
                { '=', Op::Equals   }, { 0,   Op::Contains }
            };

            int i;
            for (i = 0; ops[i].c && ops[i].c != str[opos]; ++i)
                ;

            clause.op = ops[i].op;
        }

        if (!clause.attr_name.empty() && (opos == string::npos || !clause.value.empty()))
            m_clauses.push_back(clause);
        else 
            return false;

        return true;
    }

    void parse(const string& filter_string) {
        vector<string> clause_strings;

        util::split(filter_string, ':', back_inserter(clause_strings));

        for (const string& s : clause_strings)
            if (!parse_clause(s))
                cerr << "cali-query: malformed selector clause: \"" << s << "\"" << endl;
    }

    bool match(const Attribute& attr, const Variant& data, Clause& clause) {
        if (clause.attr_id == CALI_INV_ID && clause.attr_name == attr.name())
            clause.attr_id = attr.id();

        if (clause.attr_id == attr.id())
            switch (clause.op) {
            case Op::Contains:
                return true;
            case Op::Equals:
                return clause.value == data.to_string();
            default:
                break;
            }

        return false;
    }    
    
    bool match(const CaliperMetadataDB& db, const Entry& entry, Clause& clause) {
        const Node* node = entry.node();

        if (node) {
            for ( ; node && node->id() != CALI_INV_ID; node = node->parent()) {
                Attribute attr = db.attribute(node->attribute());

                if (attr == Attribute::invalid)
                    continue;
                if (match(attr, node->data(), clause))
                    return true;
            }
        } else if (entry.attribute() != CALI_INV_ID &&
                   match(db.attribute(entry.attribute()), entry.value(), clause))
            return true;

        return false;
    }

    bool match_implicit(const CaliperMetadataDB& db, const vector<Variant>& node_ids, Clause& clause) {
        for (const Variant& elem : node_ids)
            for (const Node* node = db.node(elem.to_id()); node && node->id() != CALI_INV_ID; node = node->parent()) {
                Attribute attr = db.attribute(node->attribute());

                if (attr == Attribute::invalid)
                    continue;

                if (match(attr, node->data(), clause))
                    return true;            
            }

        return false;
    }

    bool match_explicit(const CaliperMetadataDB& db, const vector<Variant>& attr_ids, const vector<Variant>& data_entries, Clause& clause) {
        for (unsigned i = 0; i < attr_ids.size() && i < data_entries.size(); ++i) {
            Attribute attr = db.attribute(attr_ids[i].to_id());

            if (attr == Attribute::invalid)
                continue;

            if (match(attr, data_entries[i], clause))
                return true;
        }

        return false;
    }

    bool pass(const CaliperMetadataDB& db, const RecordMap& rec) {
        if (get_record_type(rec) != "ctx")
            return true;

        // implicit entries

        auto impl_entry_it = rec.find("ref");
        auto expl_entry_it = rec.find("attr");
        auto data_entry_it = rec.find("data");

        bool check_implicit = impl_entry_it != rec.end() && impl_entry_it->second.size() > 0;
        bool check_explicit = 
            expl_entry_it != rec.end() && expl_entry_it->second.size() > 0 &&
            data_entry_it != rec.end() && data_entry_it->second.size() > 0;

        for (Clause &clause : m_clauses) {
            if ((check_implicit && match_implicit(db, impl_entry_it->second, clause)) ||
                (check_explicit && match_explicit(db, expl_entry_it->second, data_entry_it->second, clause))) {
                if (clause.negate)
                    return false;
            } else if (!clause.negate)
                return false;
        }

        return true;
    }

    bool pass(const CaliperMetadataDB& db, const EntryList& list) {
        for (Clause& clause : m_clauses) {
            bool m = false;

            for (const Entry& e : list)
                if (m = (m || match(db, e, clause)))
                    break;

            if (m == clause.negate)
                return false;
        }

        return true;
    }
}; // RecordSelectorImpl


RecordSelector::RecordSelector(const string& filter_string)
    : mP { new RecordSelectorImpl }
{
    mP->parse(filter_string);
}

RecordSelector::~RecordSelector()
{
    mP.reset();
}

void
RecordSelector::operator()(CaliperMetadataDB& db, const RecordMap& rec, RecordProcessFn push) const
{
    if (mP->pass(db, rec))
        push(db, rec);
}

void 
RecordSelector::operator()(CaliperMetadataDB& db, const EntryList& list, SnapshotProcessFn push) const
{
    if (mP->pass(db, list))
        push(db, list);
}

// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// Modified by Aimee Sylvia
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

/// @file Expand.cpp
/// Print expanded records

#include "Expand.h"

#include "CaliperMetadataAccessInterface.h"

#include "Attribute.h"
#include "ContextRecord.h"
#include "Node.h"

#include "util/split.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <mutex>
#include <set>
#include <sstream>

using namespace cali;
using namespace std;

struct Expand::ExpandImpl
{
    set<string> m_selected;
    set<string> m_deselected;

    ostream&    m_os;

    std::mutex  m_os_lock;

    ExpandImpl(ostream& os)
        : m_os(os)
        { }

    void parse(const string& field_string) {
        vector<string> fields;

        util::split(field_string, ':', back_inserter(fields));

        for (const string& s : fields) {
            if (s.size() == 0)
                continue;

            if (s[0] == '-')
                m_deselected.emplace(s, 1, string::npos);
            else
                m_selected.insert(s);
        }
    }

    void print(CaliperMetadataAccessInterface& db, const RecordMap& rec) {
        int nentry = 0;

        for (auto const &entry : ContextRecord::unpack(rec, db)) {
            if (entry.second.empty())
                continue;
            if ((!m_selected.empty() && m_selected.count(entry.first) == 0) || m_deselected.count(entry.first))
                continue;

            m_os << (nentry++ ? "," : "") << entry.first << "=";

            int nelem = 0;
            for (auto it = entry.second.rbegin(); it != entry.second.rend(); ++it)
                m_os << (nelem++ ? "/" : "") << *it;
        }

        if (nentry > 0)
            m_os << endl;
    }

    void print(CaliperMetadataAccessInterface& db, const EntryList& list) {
        int nentry = 0;

        std::ostringstream os;
        
        for (const Entry& e : list) {
            if (e.node()) {
                vector<const Node*> nodes;

                for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent()) {
                    string name = db.get_attribute(node->attribute()).name();

                    if ((!m_selected.empty() && m_selected.count(name) == 0) || m_deselected.count(name))
                        continue;

                    nodes.push_back(node);
                }

                if (nodes.empty())
                    continue;

                stable_sort(nodes.begin(), nodes.end(), [](const Node* a, const Node* b) { return a->attribute() < b->attribute(); } );
	  
                cali_id_t prev_attr_id = CALI_INV_ID;

                for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
                    if ((*it)->attribute() != prev_attr_id) {
                        os << (nentry++ ? "," : "") << db.get_attribute((*it)->attribute()).name() << '=';
                        prev_attr_id = (*it)->attribute();
                    } else {
                        os << '/';
                    }
                    os << (*it)->data().to_string();
                }
            } else if (e.attribute() != CALI_INV_ID) {
                string name = db.get_attribute(e.attribute()).name();

                if ((!m_selected.empty() && m_selected.count(name) == 0) || m_deselected.count(name))
                    continue;

                os << (nentry++ ? "," : "") << name << '=' << e.value();
            }
        }
        
        if (nentry > 0) {
            std::lock_guard<std::mutex>
                g(m_os_lock);
            
            m_os << os.str() << endl;
        }
    }
};


Expand::Expand(ostream& os, const string& field_string)
    : mP { new ExpandImpl(os) }
{
    mP->parse(field_string);
}

Expand::~Expand()
{
    mP.reset();
}

void
Expand::operator()(CaliperMetadataAccessInterface& db, const RecordMap& rec) const
{
    mP->print(db, rec);
}

void 
Expand::operator()(CaliperMetadataAccessInterface& db, const EntryList& list) const
{
    mP->print(db, list);
}

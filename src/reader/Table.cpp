// Copyright (c) 2016, Lawrence Livermore National Security, LLC.  
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

/// @file Table.cpp
/// Print human-readable table

#include "Table.h"

#include "CaliperMetadataDB.h"

#include "Attribute.h"
#include "ContextRecord.h"
#include "Node.h"

#include "util/split.hpp"

#include <algorithm>
#include <iterator>

using namespace cali;

struct Table::TableImpl
{
    struct Column {
        std::string name;
        std::size_t max_width;

        Attribute   attr;

        Column(const std::string& n, std::size_t w, const Attribute& a)
            : name(n), max_width(w), attr(a)
            { }
    };

    std::vector<Column>                     m_cols;
    std::vector< std::vector<std::string> > m_rows;
    
    void parse(const std::string& field_string) {
        std::vector<std::string> fields;

        util::split(field_string, ':', std::back_inserter(fields));

        for (const std::string& s : fields)
            if (s.size() > 0)
                m_cols.emplace_back(s, s.size(), Attribute::invalid);
    }

    void add(CaliperMetadataDB& db, const EntryList& list) {
        std::vector<std::string> row(m_cols.size());
        bool active = false;
        
        for (std::vector<Column>::size_type c = 0; c < m_cols.size(); ++c) {
            if (m_cols[c].attr == Attribute::invalid)
                m_cols[c].attr = db.attribute(m_cols[c].name);
            if (m_cols[c].attr == Attribute::invalid)
                continue;

            std::string val;

            for (Entry e : list) {
                if (e.node()) {
                    for (const Node* node = e.node(); node; node = node->parent())
                        if (node->attribute() == m_cols[c].attr.id())
                            val = node->data().to_string().append(val.empty() ? "" : "/").append(val);
                } else {
                    if (e.attribute() == m_cols[c].attr.id())
                        val = e.value().to_string();
                }
            }

            if (!val.empty()) {
                active = true;
                row[c] = val;
                m_cols[c].max_width = std::max(val.size(), m_cols[c].max_width);
            }
        }

        if (active)
            m_rows.push_back(std::move(row));
    }

    void flush(std::ostream& os) {
        const char whitespace[120+1] =
            "                                        "
            "                                        "
            "                                        ";
            
        // print header

        for (const Column& col : m_cols)
            os << col.name << whitespace+(120 - std::min<std::size_t>(120, 1+col.max_width-col.name.size()));

        os << std::endl;

        // print rows

        for (auto row : m_rows) {
            for (std::vector<Column>::size_type c = 0; c < m_cols.size(); ++c)
                os << row[c] << whitespace+(120 - std::min<std::size_t>(120, 1+m_cols[c].max_width-row[c].size()));

            os << std::endl;
        }        
    }
};


Table::Table(const std::string& fields)
    : mP { new TableImpl }
{
    mP->parse(fields);
}

Table::~Table()
{
    mP.reset();
}

void 
Table::operator()(CaliperMetadataDB& db, const EntryList& list)
{
    mP->add(db, list);
}

void
Table::flush(CaliperMetadataDB&, std::ostream& os)
{
    mP->flush(os);
}

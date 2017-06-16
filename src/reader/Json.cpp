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

/// @file Json.cpp
/// Print web-readable table

#include "caliper/reader/Json.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/ContextRecord.h"
#include "caliper/common/Node.h"

#include "caliper/common/util/split.hpp"

#include <algorithm>
#include <iterator>
#include <mutex>

using namespace cali;

struct Json::JsonImpl
{
    std::vector<std::string>                m_col_attr_names;
    std::vector<Attribute>                  m_cols;    

    std::vector< std::vector<std::string> > m_rows;

    std::mutex m_col_lock;
    std::mutex m_row_lock;
    
    bool                                    m_auto_column;
    
    void parse(const std::string& field_string) {
        if (field_string.empty()) {
            m_auto_column = true;
            return;
        } else
            m_auto_column = false;
        
        util::split(field_string, ':', std::back_inserter(m_col_attr_names));
    }

    void update_column_attribute(CaliperMetadataAccessInterface& db, cali_id_t attr_id) {
        auto it = std::find_if(m_cols.begin(), m_cols.end(),
                               [attr_id](const Attribute& c) {
                                   return c.id() == attr_id;
                               });

        if (it != m_cols.end())
            return;
        
        Attribute attr   = db.get_attribute(attr_id);

        if (attr == Attribute::invalid)
            return;
        
        std::string name = attr.name();

        // Skip internal "cali." and ".event" attributes
        if (name.compare(0, 5, "cali." ) == 0 ||
            name.compare(0, 6, "event.") == 0)
            return;
                
        m_cols.push_back(attr);
    }
    
    std::vector<Attribute> update_columns(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_col_lock);
        
        // Auto-generate columns from attributes in the snapshots. Used if no
        // field list was given. Skips some internal attributes.

        if (m_auto_column) {
            for (Entry e : list) {
                if (e.node()) {
                    for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent())
                        update_column_attribute(db, node->attribute());
                } else
                    update_column_attribute(db, e.attribute());
            }
        } else {
            // Check if we can look up attribute object from name

            auto it = m_col_attr_names.begin();

            while (it != m_col_attr_names.end()) {
                Attribute attr = db.get_attribute(*it);

                if (attr != Attribute::invalid) {
                    m_cols.push_back(attr);
                    it = m_col_attr_names.erase(it);
                } else
                    ++it;
            }
        }

        return m_cols;
    }
    
    void add(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::vector<Attribute>   col(update_columns(db, list));
        std::vector<std::string> row(col.size());

        bool active = false;
        
        for (std::vector<Attribute>::size_type c = 0; c < col.size(); ++c) {
            if (col[c] == Attribute::invalid)
                continue;

            std::string val;

            for (Entry e : list) {
                if (e.node()) {
                    for (const Node* node = e.node(); node; node = node->parent())
                        if (node->attribute() == col[c].id())
                            val = node->data().to_string().append(val.empty() ? "" : "/").append(val);
                } else {
                    if (e.attribute() == col[c].id())
                        val = e.value().to_string();
                }
            }

            if (!val.empty()) {
                active = true;
                row[c] = val;
            }
        }

        if (active) {
            std::lock_guard<std::mutex>
                g(m_row_lock);
            
            m_rows.push_back(std::move(row));
        }
    }

    void flush(std::ostream& os) {            
        // print header
        os << "{ \"attributes\" : [" ;
        for (const Attribute& col : m_cols)
            os << "\"" << col.name() <<"\",";

        os << "\b ], "<< std::endl<<" \"rows\" : [ ";

        // print rows

        for (auto row : m_rows) {
            os << " [ " ;
            for (std::vector<std::string>::size_type c = 0; c < row.size(); ++c) {
                    os << "\"" << row[c] << "\",";
            }
            os<< "\b ]," ;
        }        
        os << "\b]}" << std::endl;
    }
};


Json::Json(const std::string& fields)
    : mP { new JsonImpl }
{
    mP->parse(fields);
}

Json::~Json()
{
    mP.reset();
}

void 
Json::operator()(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->add(db, list);
}

void
Json::flush(CaliperMetadataAccessInterface&, std::ostream& os)
{
    mP->flush(os);
}

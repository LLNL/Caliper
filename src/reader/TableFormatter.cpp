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

// Print human-readable table

#include "caliper/reader/TableFormatter.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"
#include "caliper/common/StringConverter.h"

#include "caliper/common/util/split.hpp"

#include "../common/util/format_util.h"

#include <algorithm>
#include <iterator>
#include <mutex>

using namespace cali;

struct TableFormatter::TableImpl
{
    struct Column {
        std::string name;
        std::string display_name;
        std::size_t max_width;

        Attribute   attr;

        bool        print; // used for hidden sort columns

        QuerySpec::SortSpec::Order sort_order;

        Column(const std::string& n, const std::string alias, std::size_t w, const Attribute& a, bool p,
               QuerySpec::SortSpec::Order o = QuerySpec::SortSpec::Order::None)
            : name(n), display_name(alias), max_width(w), attr(a), print(p), sort_order(o)
            { }
    };

    std::vector<Column>                     m_cols;
    std::vector< std::vector<std::string> > m_rows;

    std::mutex                              m_col_lock;
    std::mutex                              m_row_lock;

    std::map<std::string, std::string>      m_aliases;

    bool                                    m_auto_column;

    void parse(const std::string& field_string, const std::string& sort_string) {
        std::vector<std::string> fields;

        // fill sort columns

        util::split(sort_string, ':', std::back_inserter(fields));

        for (const std::string& s : fields)
            if (s.size() > 0)
                m_cols.emplace_back(s, s, s.size(), Attribute::invalid, false);

        fields.clear();

        // fill print columns

        if (field_string.empty()) {
            m_auto_column = true;
            return;
        } else
            m_auto_column = false;

        util::split(field_string, ':', std::back_inserter(fields));

        for (const std::string& s : fields)
            if (s.size() > 0)
                m_cols.emplace_back(s, s, s.size(), Attribute::invalid, true);
    }

    void configure(const QuerySpec& spec) {
        m_cols.clear();
        m_rows.clear();
        
        m_auto_column = false;

        m_aliases = spec.aliases;

        // Fill sort columns

        switch (spec.sort.selection) {
        case QuerySpec::SortSelection::Default:
        case QuerySpec::SortSelection::None:
        case QuerySpec::SortSelection::All:
            break;
        case QuerySpec::SortSelection::List:
            for (const QuerySpec::SortSpec& s : spec.sort.list)
                m_cols.emplace_back(s.attribute, s.attribute, s.attribute.size(), Attribute::invalid, false, s.order);
            break;
        }

        // Fill header columns
        
        switch (spec.attribute_selection.selection) {
        case QuerySpec::AttributeSelection::Default:
        case QuerySpec::AttributeSelection::All:
            m_auto_column = true;
            break;
        case QuerySpec::AttributeSelection::List:
            for (const std::string& s : spec.attribute_selection.list) {
                std::string alias = s;
                
                auto it = m_aliases.find(s);
                if (it != m_aliases.end())
                    alias = it->second;
                    
                m_cols.emplace_back(s, alias, alias.size(), Attribute::invalid, true);
            }
            break;
        case QuerySpec::AttributeSelection::None:
            // Keep auto_column = false and empty column list
            break;
        }
    }
    
    void update_column_attribute(CaliperMetadataAccessInterface& db, cali_id_t attr_id) {
        auto it = std::find_if(m_cols.begin(), m_cols.end(),
                               [attr_id](const Column& c) {
                                   return c.sort_order == QuerySpec::SortSpec::None && c.attr.id() == attr_id;
                               });

        if (it != m_cols.end())
            return;

        Attribute attr = db.get_attribute(attr_id);

        if (attr == Attribute::invalid)
            return;

        std::string name = attr.name();

        // Skip internal "cali." and ".event" attributes
        if (name.compare(0, 5, "cali." ) == 0 ||
            name.compare(0, 6, "event.") == 0)
            return;

        std::string alias = name;
        
        {
            auto ait = m_aliases.find(name);
            if (ait != m_aliases.end())
                alias = ait->second;
        }
        
        m_cols.emplace_back(name, alias, alias.size(), attr, true);
    }

    std::vector<Column> update_columns(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::lock_guard<std::mutex>
            g(m_col_lock);

        // Auto-generate columns from attributes in the snapshots. Used if no
        // field list was given. Skips some internal attributes.

        if (m_auto_column)
            for (Entry e : list) {
                if (e.node()) {
                    for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent())
                        update_column_attribute(db, node->attribute());
                } else
                    update_column_attribute(db, e.attribute());
            }

        // Check if we can look up attribute object from name

        for (Column& col : m_cols)
            if (col.attr == Attribute::invalid)
                col.attr = db.get_attribute(col.name);

        return m_cols;
    }

    void add(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::vector<Column> cols = update_columns(db, list);
        std::vector<std::string> row(cols.size());

        bool active = false;
        bool update_max_width = false;

        for (std::vector<Column>::size_type c = 0; c < cols.size(); ++c) {
            if (cols[c].attr == Attribute::invalid)
                continue;

            std::string val;

            for (Entry e : list) {
                if (e.node()) {
                    std::string str;

                    for (const Node* node = e.node(); node; node = node->parent())
                        if (node->attribute() == cols[c].attr.id())
                            str = node->data().to_string().append(str.empty() ? "" : "/").append(str);

                    if (!str.empty()) {
                        val = str;
                        break;
                    }
                } else if (e.attribute() == cols[c].attr.id()) {
                    val = e.value().to_string();
                    break;
                }
            }

            if (!val.empty()) {
                active = true;
                row[c] = val;

                size_t width = val.length();

                if (width > cols[c].max_width) {
                    cols[c].max_width = width;
                    update_max_width  = true;
                }
            }
        }

        if (active) {
            std::lock_guard<std::mutex>
                g(m_row_lock);

            m_rows.push_back(std::move(row));
        }

        if (update_max_width) {
            std::lock_guard<std::mutex>
                g(m_col_lock);

            for (std::vector<Column>::size_type c = 0; c < cols.size(); ++c)
                if (cols[c].max_width > m_cols[c].max_width)
                    m_cols[c].max_width = cols[c].max_width;
        }
    }

    void flush(std::ostream& os) {
        // NOTE: No locking, assume flush() runs serially

        // sort rows
        // NOTE: This is REALLY slow (potentially converts strings to numbers on every comparison)

        for (std::vector<Column>::size_type c = 0; c < m_cols.size(); ++c)
            if (m_cols[c].sort_order == QuerySpec::SortSpec::Order::Ascending)
                std::stable_sort(m_rows.begin(), m_rows.end(),
                                 [c,this](const std::vector<std::string>& lhs, const std::vector<std::string>& rhs){
                                     if (c >= lhs.size() || c >= rhs.size())
                                         return lhs.size() < rhs.size();
                                     cali_attr_type type = this->m_cols[c].attr.type();
                                     return Variant::from_string(type, lhs[c].c_str()) < Variant::from_string(type, rhs[c].c_str());
                                 });
            else if (m_cols[c].sort_order == QuerySpec::SortSpec::Order::Descending)
                std::stable_sort(m_rows.begin(), m_rows.end(),
                                 [c,this](const std::vector<std::string>& lhs, const std::vector<std::string>& rhs){
                                     if (c >= lhs.size() || c >= rhs.size())
                                         return lhs.size() > rhs.size();
                                     cali_attr_type type = this->m_cols[c].attr.type();
                                     return Variant::from_string(type, lhs[c].c_str()) > Variant::from_string(type, rhs[c].c_str());
                                 });                

        // print header

        for (const Column& col : m_cols)
            if (col.print)
                util::pad_right(os, col.display_name, col.max_width);

        os << std::endl;

        // print rows

        for (auto row : m_rows) {
            for (std::vector<Column>::size_type c = 0; c < row.size(); ++c) {
                if (!m_cols[c].print)
                    continue;

                std::string    str = row[c];
                cali_attr_type t   = m_cols[c].attr.type();
                bool           align_right = (t == CALI_TYPE_INT  ||
                                              t == CALI_TYPE_UINT ||
                                              t == CALI_TYPE_DOUBLE);

                if (align_right)
                    util::pad_left (os, str, m_cols[c].max_width);
                else
                    util::pad_right(os, str, m_cols[c].max_width);
            }

            os << std::endl;
        }
    }
};


TableFormatter::TableFormatter(const std::string& fields, const std::string& sort_fields)
    : mP { new TableImpl }
{
    mP->parse(fields, sort_fields);
}

TableFormatter::TableFormatter(const QuerySpec& spec)
    : mP { new TableImpl }
{
    mP->configure(spec);
}

TableFormatter::~TableFormatter()
{
    mP.reset();
}

void
TableFormatter::process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->add(db, list);
}

void
TableFormatter::flush(CaliperMetadataAccessInterface&, std::ostream& os)
{
    mP->flush(os);
}

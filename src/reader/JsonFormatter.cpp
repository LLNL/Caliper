// Copyright (c) 2016, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by Alfredo Gimenez, gimenez1@llnl.gov.
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

// Print web-readable table in sparse format

#include "caliper/reader/JsonFormatter.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/ContextRecord.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include "caliper/common/util/split.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <mutex>
#include <set>
#include <sstream>
#include <iostream>

using namespace cali;
using namespace std;

const std::string opt_split = std::string("split");
const std::string opt_pretty = std::string("pretty");
const std::string opt_quote_all = std::string("quote-all");

bool g_first_row = true;

struct JsonFormatter::JsonFormatterImpl
{
    set<string>  m_selected;
    set<string>  m_deselected;

    OutputStream m_os;

    std::mutex   m_os_lock;

    bool         m_opt_split = false;
    bool         m_opt_pretty = false;
    bool         m_opt_quote_all = false;

    JsonFormatterImpl(OutputStream &os)
        : m_os(os)
        { }

    void parse(const string& field_string) {
        vector<string> fields;

        util::split(field_string, ':', back_inserter(fields));

        for (const string& s : fields) {
            if (s.size() == 0)
                continue;

            if (s[0] == '-')
                m_deselected.insert(s.substr(1, string::npos));
            else
                m_selected.insert(s);
        }
    }


    void configure(const QuerySpec& spec) {
        for (auto arg : spec.format.args) {
            if (arg == opt_split)
                m_opt_split = true;
            if (arg == opt_pretty)
                m_opt_pretty = true;
            if (arg == opt_quote_all)
                m_opt_quote_all = true;
        }

        switch (spec.attribute_selection.selection) {
        case QuerySpec::AttributeSelection::Default:
        case QuerySpec::AttributeSelection::All:
            // do nothing; default is all
            break;
        case QuerySpec::AttributeSelection::None:
            // doesn't make much sense
            break;
        case QuerySpec::AttributeSelection::List:
            m_selected =
                std::set<std::string>(spec.attribute_selection.list.begin(),
                                      spec.attribute_selection.list.end());
            break;
        }
    }
    
    void print(CaliperMetadataAccessInterface& db, const EntryList& list) {

        std::vector<std::string> key_value_pairs;
        std::ostringstream ss_key_value;

        std::ostringstream os;

        bool writing_attr_data = false;
        
        for (const Entry& e : list) {

            if (e.node()) {

                // First find all nodes selected for printing
                vector<const Node*> nodes;
                for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent()) {
                    Attribute attr = db.get_attribute(node->attribute());
                    string name = attr.name();

                    if (attr.is_hidden() || (!m_selected.empty() && m_selected.count(name) == 0) || m_deselected.count(name))
                        continue;

                    nodes.push_back(node);
                }

                if (nodes.empty())
                    continue;

                // Sort all nodes consistently baseed on attribute id
                stable_sort(nodes.begin(), nodes.end(), [](const Node* a, const Node* b) { return a->attribute() < b->attribute(); } );
	  
                cali_id_t prev_attr_id = CALI_INV_ID;

                bool quotes = m_opt_quote_all | false;

                // Go through all nodes in reverse order
                for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {

                    // Get attribute information
                    cali_id_t attr_id = (*it)->attribute();
                    Attribute attr = db.get_attribute(attr_id);
                    cali_attr_type attr_type = attr.type();

                    // Check if we encountered a new attribute
                    if (attr_id != prev_attr_id) {

                        // Check if we are completing a previous attribute
                        if (writing_attr_data) {
                            // Complete a pair and push it back, then clear the stringstream
                            ss_key_value << (quotes ? "\"" : "");
                            key_value_pairs.push_back(ss_key_value.str());
                            ss_key_value.str(std::string());
                        }

                        // Start new attribute
                        quotes = m_opt_quote_all | false;
                        ss_key_value << '"' << attr.name() << '"' << ':';

                        // If STRING or USR, or if nested attribute values x/y/z, start quotes
                        if (attr_type == CALI_TYPE_STRING || attr_type == CALI_TYPE_USR 
                                || ((it+1 != nodes.rend()) && (*(it+1))->attribute()) != attr_id) {
                            quotes = true;
                        }

                        if (quotes) {
                            ss_key_value << '"';
                        }

                        writing_attr_data = true;
                        prev_attr_id = attr_id;

                    } else {
                        ss_key_value << '/';
                    }

                    ss_key_value << (*it)->data().to_string();
                }

                if (writing_attr_data) {
                    ss_key_value << (quotes ? "\"" : "");
                    key_value_pairs.push_back(ss_key_value.str());
                    ss_key_value.str(std::string());
                    writing_attr_data = false;
                    quotes = m_opt_quote_all | false;
                }

            } else if (e.attribute() != CALI_INV_ID) {
                Attribute attr = db.get_attribute(e.attribute());
                string name = attr.name();
                cali_attr_type attr_type = attr.type();

                // Check if this attribute is selected for printing
                if (attr.is_hidden() || (!m_selected.empty() && m_selected.count(name) == 0) || m_deselected.count(name))
                    continue;

                bool quotes = m_opt_quote_all | false;

                ss_key_value << '"' << name << '"' << ':' ;
                
                string data = e.value().to_string();
                if (attr_type == CALI_TYPE_STRING || attr_type == CALI_TYPE_USR)
                    quotes = true;

                if (quotes)
                    ss_key_value << '"' << data << '"';
                else
                    ss_key_value << data;

                key_value_pairs.push_back(ss_key_value.str());
                ss_key_value.str(std::string());
            }
        }

        if (!m_opt_split) 
            os << (g_first_row ? "[\n" : ",");

        os << (g_first_row ? "" : "\n") << "{" << (m_opt_pretty ? "\n\t" : "");

        for(size_t i = 0; i < key_value_pairs.size(); ++i)
        {
            if(i != 0)
                os << "," << (m_opt_pretty ? "\n\t" : "");
            os << key_value_pairs[i];
        }

        os << (m_opt_pretty ? "\n" : "" ) << "}";
        
        if (!key_value_pairs.empty()) {
            std::lock_guard<std::mutex>
                g(m_os_lock);
            
            m_os.stream() << os.str();
            g_first_row = false;
        }
    }
};

JsonFormatter::JsonFormatter(OutputStream &os, const QuerySpec& spec)
    : mP { new JsonFormatterImpl(os) }
{
    mP->configure(spec);
}

JsonFormatter::~JsonFormatter()
{
    mP.reset();
}

void 
JsonFormatter::process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->print(db, list);
}

void JsonFormatter::flush(CaliperMetadataAccessInterface&, std::ostream& os)
{
    if (!mP->m_opt_split)
        os << "\n]";
}


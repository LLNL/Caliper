// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
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

#include "query_common.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/tools-util/Args.h"

#include "caliper/common/Log.h"
#include "caliper/common/util/split.hpp"

#include <cctype>
#include <iterator>
#include <sstream>

using namespace cali;
using namespace util;

namespace
{

inline bool
is_one_of(char c, const char* characters)
{
    for (const char* ptr = characters; *ptr != '\0'; ++ptr)
        if (*ptr == c)
            return true;

    return false;
}

/// \brief Read next character, ignoring whitespace
char
parse_char(std::istream& is)
{
    char ret = '\0';

    do {
        ret = is.get();
    } while (is.good() && isspace(ret));
    
    return ret;
}

/// \brief Parse text from stream, ignoring whitespace, until one of ",()" are found
std::string
parse_word(std::istream& is)
{
    std::string ret;

    while (is.good()) {
        auto c = is.get();

        if (!is.good())
            break;
        if (isspace(c))
            continue;
        if (is_one_of(c, ",()\n")) {
            is.unget();
            break;
        }

        ret.push_back(c);
    }

    return ret;
}

/// \brief Parse "(arg1, arg2, ...)" argument list, ignoring whitespace
std::vector<std::string>
parse_arglist(std::istream& is)
{
    std::vector<std::string> ret;
    std::string word;

    char c = parse_char(is);

    if (!is.good())
        return ret;

    if (c != '(') {
        is.unget();
        return ret;
    }
    
    do {
        std::string str = parse_word(is);
        c = parse_char(is);

        if (!str.empty() && (c == ',' || c == ')'))
            ret.push_back(str);
    } while (is.good() && c == ',');

    if (c != ')') {
        is.unget();
        ret.clear();
    }
    
    return ret;
}

std::pair< int, std::vector<std::string> >
parse_functioncall(std::istream& is, const QuerySpec::FunctionSignature* defs)
{
    // read function name
    std::string fname = parse_word(is);

    if (fname.empty())
        return std::make_pair(-1, std::vector<std::string>());

    // find function among given signatures    
    int retid = 0;

    for ( ; defs && defs[retid].name && (fname != defs[retid].name); ++retid)
        ;

    if (!defs || !defs[retid].name) {
        Log(0).stream() << "Unknown function \"" << fname << "\"" << std::endl;
        return std::make_pair(-1, std::vector<std::string>());
    }

    // read argument list    
    std::vector<std::string> args = parse_arglist(is);

    if (args.size() < defs[retid].min_args || args.size() > defs[retid].max_args) {
        Log(0).stream() << "Error: Expected " << defs[retid].min_args
                        << " arguments for function \"" << defs[retid].name
                        << "\" but got " << args.size() << std::endl;

        return std::make_pair(-1, std::vector<std::string>());
    }

    return std::make_pair(retid, std::move(args));
}

}

namespace cali
{

bool
QueryArgsParser::parse_args(const Args& args)
{
    m_spec.filter.selection              = QuerySpec::FilterSelection::Default;
    m_spec.attribute_selection.selection = QuerySpec::AttributeSelection::Default;
    m_spec.aggregation_ops.selection     = QuerySpec::AggregationSelection::None;
    m_spec.aggregation_key.selection     = QuerySpec::AttributeSelection::None;
    m_spec.sort.selection                = QuerySpec::SortSelection::Default;
    m_spec.format.opt                    = QuerySpec::FormatSpec::Default;

    m_error = false;
    m_error_msg.clear();
    
    // parse CalQL query (if any)

    if (args.is_set("query")) {
        std::string q = args.get("query");
        CalQLParser p(q.c_str());

        if (p.error()) {
            m_error     = true;
            m_error_msg = p.error_msg();
            return false;
        } else
            m_spec = p.spec();
    }
    
    // setup filter

    if (args.is_set("select")) {
        m_spec.filter.selection = QuerySpec::FilterSelection::List;
        m_spec.filter.list      = RecordSelector::parse(args.get("select"));
    }
    
    // setup attribute selection
    
    if (args.is_set("attributes")) {
        m_spec.attribute_selection.selection = QuerySpec::AttributeSelection::List;
        util::split(args.get("attributes"), ',', std::back_inserter(m_spec.attribute_selection.list));
    }
    
    // setup aggregation
    
    if (args.is_set("aggregate")) {
        // aggregation ops
        m_spec.aggregation_ops.selection = QuerySpec::AggregationSelection::Default;

        std::string opstr = args.get("aggregate");

        if (!opstr.empty()) {
            m_spec.aggregation_ops.selection = QuerySpec::AggregationSelection::List;

            std::istringstream is(opstr);
            char c;

            const QuerySpec::FunctionSignature* defs = Aggregator::aggregation_defs();
            
            do {
                auto fpair = parse_functioncall(is, defs);

                if (fpair.first >= 0)
                    m_spec.aggregation_ops.list.emplace_back(defs[fpair.first], fpair.second, "");

                c = parse_char(is);
            } while (is.good() && c == ',');
        }

        // aggregation key 
        m_spec.aggregation_key.selection = QuerySpec::AttributeSelection::Default;
        
        if (args.is_set("aggregate-key")) {
            std::string keystr = args.get("aggregate-key");

            if (keystr == "none") {
                m_spec.aggregation_key.selection = QuerySpec::AttributeSelection::None;
            } else {
                m_spec.aggregation_key.selection = QuerySpec::AttributeSelection::List;
                util::split(keystr, ',', std::back_inserter(m_spec.aggregation_key.list));
            }
        }
    }
    
    // setup sort
    
    if (args.is_set("sort")) {
        m_spec.sort.selection = QuerySpec::SortSelection::List;
        
        std::vector<std::string> list;
        util::split(args.get("sort"), ',', std::back_inserter(list));

        for (const std::string& s : list)
            m_spec.sort.list.emplace_back(s);
    }

    // setup formatter
    
    for (const QuerySpec::FunctionSignature* fmtsig = FormatProcessor::formatter_defs(); fmtsig && fmtsig->name; ++fmtsig) {
        // see if a formatting option is set        
        if (args.is_set(fmtsig->name)) {
            m_spec.format.opt       = QuerySpec::FormatSpec::User;
            m_spec.format.formatter = *fmtsig;

            // Find formatter args (if any)
            for (int i = 0; i < fmtsig->max_args; ++i)
                if (args.is_set(fmtsig->args[i])) {
                    m_spec.format.args.resize(i+1);
                    m_spec.format.args[i] = args.get(fmtsig->args[i]);
                }

            // NOTE: This check isn't complete yet.
            if (m_spec.format.args.size() < fmtsig->min_args) {                
                m_error     = true;
                m_error_msg = std::string("Insufficient arguments for formatter ") + fmtsig->name;

                return false;
            }
            
            break;
        }
    }
        
    return true;
}

}

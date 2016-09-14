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

/// @file Format.cpp
/// Print expanded records

#include "Format.h"

#include "CaliperMetadataDB.h"

#include "Attribute.h"
#include "ContextRecord.h"
#include "Node.h"

#include "util/split.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <set>


using namespace cali;
using namespace std;

struct Format::FormatImpl
{
    struct Field {
        string    prefix;
        
        string    attr_name;
        Attribute attr;

        size_t    width;
        char      align; // 'l', 'r', 'c'
    }; // for the frmtparse function

    vector<Field> m_fields; // for the format strings
    string        m_title;

    ostream&      m_os;

    FormatImpl(ostream& os)
        : m_os(os)
        { }

    // based on SnapshotTextFormatter::parse
    void parse(const string& formatstring) {

        // parse format: "(prefix string)%[<width+alignment(l|r|c)>]attr_name%..."
        vector<string> split_string;

        util::split(formatstring, '%', back_inserter(split_string));

        while (!split_string.empty()) {
            Field field = { "", "", Attribute::invalid, 0, 'l' };

            field.prefix = split_string.front();
            split_string.erase(split_string.begin());
            
            // parse field entry

            vector<string> field_strings;
            util::tokenize(split_string.front(), "[]", back_inserter(field_strings));

            // look for width/alignment specification (in [] brackets)

            int wfbegin = -1;
            int wfend   = -1;
            int apos    = -1;

            int nfields = field_strings.size();

            for (int i = 0; i < nfields; ++i)
                if(field_strings[i] == "[")
                    wfbegin = i;
                else if (field_strings[i] == "]")
                    wfend = i;

            if (wfbegin >= 0 && wfend > wfbegin+1) {
                // width field specified
                field.width = stoi(field_strings[wfbegin+1]);

                if (wfbegin > 0)
                    apos = 0;
                else if (wfend+1 < nfields)
                    apos = wfend+1;
            } else if (nfields > 0)
                apos = 0;

            if (apos >= 0)
                field.attr_name = field_strings[apos];

            split_string.erase(split_string.begin());

            m_fields.push_back(field);
        }
    }

    void print(CaliperMetadataDB& db, const EntryList& list) {        
        for (Field f : m_fields) {
            if (f.attr == Attribute::invalid)
                f.attr = db.attribute(f.attr_name);
            
            string str;

            if (f.attr != Attribute::invalid)
                for (const Entry& e: list) {
                    if (e.node()) {
                        for (const Node* node = e.node(); node; node = node->parent())
                            if (node->attribute() == f.attr.id())
                                str = node->data().to_string().append(str.size() ? "/" : "").append(str);
                    } else if (e.attribute() == f.attr.id())
                        str.append(e.value().to_string());

                    if (!str.empty())
                        break;
                }
            
            const char whitespace[80+1] =
                "                                        "
                "                                        ";

            size_t len = str.size();
            size_t w   = len < f.width ? std::min<size_t>(f.width - len, 80) : 0;

            m_os << f.prefix << str << (w > 0 ? whitespace+(80-w) : "");
        }

        m_os << std::endl;
    }
};


Format::Format(ostream& os, const string& formatstr, const string& titlestr)
    : mP { new FormatImpl(os) }
{
    mP->parse(formatstr);    
    mP->m_os << titlestr;
}

Format::~Format()
{
    mP.reset();
}

void 
Format::operator()(CaliperMetadataDB& db, const EntryList& list) const
{
    mP->print(db, list);
}

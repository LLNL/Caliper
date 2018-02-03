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

/// @file CsvSpec.cpp
/// CsvSpec implementation

#include "caliper/common/csv/CsvSpec.h"

#include "caliper/common/Record.h"
#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"

#include "../util/write_util.h"

#include <vector>

using namespace cali;
using namespace std;

namespace 
{

struct CsvSpecImpl
{
    static CsvSpecImpl s_caliper_csvspec;

    const char  m_sep       { ','      }; ///< separator character
    char        m_esc       { '\\'     };
    const char* m_esc_chars { "\\,=\n" }; ///< characters that need to be escaped

    // --- read interface

    vector<string> split(const string& line, char sep, bool keep_escape = false) const {
        vector<string> vec;
        string str;
        bool   escaped = false;
        
        for (auto it = line.begin(); it != line.end(); ++it) {
            if (!escaped && *it == m_esc) {
                escaped = true;
                
                if (keep_escape)
                    str.push_back(m_esc);
            } else if (!escaped && *it == sep) {
                vec.push_back(str);
                str.clear();
            } else {
                str.push_back(*it);
                escaped = false;
            }
        }

        vec.push_back(str);

        return vec;
    }

    void write_record(ostream& os, const RecordDescriptor& record, const int count[], const Variant* data[]) {
        os << "__rec=" << record.name;

        for (unsigned e = 0; e < record.num_entries; ++e) {
            if (count[e] > 0)
                os << "," << record.entries[e];
            for (int c = 0; c < count[e]; ++c)
                util::write_esc_string(os << '=', data[e][c].to_string(), m_esc_chars);
        }

        os << endl;
    }

    void write_record(ostream& os, const RecordMap& record) {
        int count = 0;

        for (const auto &entry : record) {
            if (!entry.second.empty())
                os << (count++ ? "," : "") << entry.first;
            for (const auto &elem : entry.second)
                util::write_esc_string(os << '=', elem, m_esc_chars);
        }

        if (count)
            os << endl;
    }

    RecordMap read_record(const string& line) {
        vector<string> entries = split(line, m_sep, true /* keep escape */ );
        RecordMap      rec;

        for (const string& entry : entries) {
            vector<string> keyval = split(entry, '=', false);

            if (keyval.size() > 1) {
                vector<std::string> data;

                for (auto it = keyval.begin()+1; it != keyval.end(); ++it)
                    data.emplace_back(*it);

                rec.insert(make_pair(keyval[0], std::move(data)));                
            } else
                Log(1).stream() << "Invalid CSV entry: " << entry << endl;
        }

        return rec;
    }
};

CsvSpecImpl CsvSpecImpl::s_caliper_csvspec;

} // namespace

void
CsvSpec::write_record(ostream& os, const RecordMap& record)
{
    ::CsvSpecImpl::s_caliper_csvspec.write_record(os, record);
}

void
CsvSpec::write_record(ostream& os, const RecordDescriptor& record, const int* data_count, const Variant** data)
{
    ::CsvSpecImpl::s_caliper_csvspec.write_record(os, record, data_count, data);
}

RecordMap 
CsvSpec::read_record(const string& line)
{
    return ::CsvSpecImpl::s_caliper_csvspec.read_record(line);
}

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

/// @file CsvReader.cpp
/// CsvReader implementation

#include "caliper/common/csv/CsvReader.h"

#include "caliper/common/Log.h"

#include <iostream>
#include <fstream>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

vector<string> split(const string& line, char sep, bool keep_escape = false) {
    vector<string> vec;
    string str;
    bool   escaped = false;
        
    for (auto it = line.begin(); it != line.end(); ++it) {
        if (!escaped && *it == '\\') {
            escaped = true;
                
            if (keep_escape)
                str.push_back('\\');
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

RecordMap read_record(const string& line) {
    vector<string> entries = split(line, ',', true /* keep escape */ );
    RecordMap      rec;

    for (const string& entry : entries) {
        vector<string> keyval = split(entry, '=', false);

        if (keyval.size() > 1) {
            vector<std::string> data;

            for (auto it = keyval.begin()+1; it != keyval.end(); ++it)
                data.emplace_back(*it);

            rec.insert(make_pair(keyval[0], std::move(data)));                
        } else
            Log(0).stream() << "Invalid CSV entry: " << entry << endl;
    }

    return rec;
}

} // namespace [anonymous]

struct CsvReader::CsvReaderImpl
{
    string m_filename;

    CsvReaderImpl(const string& filename)
        : m_filename { filename }
        { }

    bool read(function<void(const RecordMap&)> rec_handler) {
        if (m_filename.empty()) {
            // empty file: read from stdin

            for (string line ; getline(std::cin, line); )
                rec_handler(::read_record(line));
        } else {
            // read from file

            ifstream is(m_filename.c_str());

            if (!is)
                return false;

            for (string line ; getline(is, line); )
                rec_handler(::read_record(line));
        }

        return true;
    }
};

CsvReader::CsvReader(const string& filename)
    : mP { new CsvReaderImpl(filename) }
{ }

CsvReader::~CsvReader()
{ }

bool
CsvReader::read(function<void(const RecordMap&)> rec_handler)
{
    return mP->read(rec_handler);
}

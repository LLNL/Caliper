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

///@file Args.cpp
/// Argument parsing implementation

#include "caliper/tools-util/Args.h"

#include <cstring>
#include <map>

using namespace std;
using namespace util;

const Args::Table Args::Table::Terminator { 0, 0, 0, false, 0, 0 };

struct Args::ArgsImpl
{
    vector<Args::Table> m_options;     ///< Option list

    map<string, vector<Table>::size_type> m_option_map;     ///< Option key -> option index

    map<string, vector<Table>::size_type> m_long_options;   ///< Long option name -> option index
    map<char,   vector<Table>::size_type> m_short_options;  ///< Short option name -> option index

    map<vector<Table>::size_type, string> m_active_options; ///< Option index -> option argument

    vector<string>      m_arguments;   ///< Non-option arguments (filenames etc.)
    string              m_programname; ///< argv[0]

    bool                m_fail;        ///< Fail when parsing unknown option

    string              m_shortopt_prefix { "-"  };
    string              m_longopt_prefix  { "--" };
    string              m_options_end     { "--" }; 

    void add_table(const Table table[]) {
        for (const Table* t = table; t->name; ++t) {
            auto n = m_options.size();

            m_options.push_back(*t);

            if (t->longopt)
                m_long_options.insert(make_pair(string(t->longopt), n));
            if (t->shortopt)
                m_short_options.insert(make_pair(t->shortopt, n));

            m_option_map.insert(make_pair(string(t->name), n));
        }
    }

    int parse(int argc, const char* const argv[], int pos) {
        if (argc <= pos)
            return pos;

        m_programname = argv[0];

        int i;

        for (i = pos; i < argc; ++i) {
            string arg { argv[i] };

            if (arg.empty())
                continue;
            if (arg == m_options_end)
                break;

            // --- handle long options

            auto lps = m_longopt_prefix.size();

            if (!m_longopt_prefix.empty() && arg.find(m_longopt_prefix) == 0) {
                string::size_type dpos = arg.find('=');

                auto it = m_long_options.find(arg.substr(lps, dpos == string::npos ? dpos : dpos-lps));

                if (it == m_long_options.end())
                    return i;

                string optarg;

                if (dpos != string::npos && dpos+1 < arg.size())
                    optarg.assign(arg, dpos+1, string::npos);
                if (m_options[it->second].has_argument && optarg.empty() && i+1 < argc)
                    optarg.assign(argv[++i]);

                m_active_options.insert(make_pair(it->second, optarg));

                continue;
            }

            // handle short options

            auto sps = m_shortopt_prefix.size();

            if (!m_shortopt_prefix.empty() && arg.find(m_shortopt_prefix) == 0) {
                string::size_type dpos = arg.find('=');

                // handle characters in the shortopt string; last one may have an option argument 
                for (auto n = sps ; n < arg.size() && n < dpos; ++n) {
                    auto it = m_short_options.find(arg[n]);

                    if (it == m_short_options.end())
                        return i;

                    string optarg;

                    if (dpos+1 < arg.size() && n == dpos-1)
                        optarg.assign(arg, dpos+1, string::npos);
                    if (n == arg.size()-1 && m_options[it->second].has_argument && (i+1) < argc)
                        optarg.assign(string(argv[++i]));

                    m_active_options.insert(make_pair(it->second, optarg));
                }

                continue;
            }

            // this is a non-option argument
            m_arguments.emplace_back(arg);
        }

        // treat all remaining elements as arguments (i.e., not options)

        for (++i; i < argc; ++i)
            m_arguments.emplace_back(argv[i]);

        return i;
    }

    string get(const string& name, const string& def) const {
        auto opt_it = m_option_map.find(name);
        
        if (opt_it == m_option_map.end())
            return def;

        auto it = m_active_options.find(opt_it->second);

        return it == m_active_options.end() ? def : it->second;
    }

    bool is_set(const string& name) const {
        auto opt_it = m_option_map.find(name);
        
        if (opt_it == m_option_map.end())
            return false;

        auto it = m_active_options.find(opt_it->second);

        return it != m_active_options.end();
    }

    vector<string> options() const {
        vector<string> out;

        for (auto const &p : m_active_options)
            out.emplace_back(m_options[p.first].name);

        return out;
    }

    vector<string> arguments() const {
        return m_arguments;
    }

    void print_available_options(ostream& os) const {
        const string::size_type pad         = 2;
        string::size_type       max_longopt = m_longopt_prefix.size();

        // Get max longopt+argument info size for padding
        for (auto const& l : m_long_options) {
            string::size_type s = m_longopt_prefix.size() 
                + l.first.size() 
                + (m_options[l.second].argument_info ? strlen(m_options[l.second].argument_info) + 1 : 0);

            max_longopt = max(max_longopt, s);
        }

        const string opt_sep(", ");
        const char*  whitespace = "                                        ";
        const string::size_type maxwhitespace = strlen(whitespace);

        for (const Table& opt : m_options) {
            os.write(whitespace, pad);

            if (opt.shortopt)
                os << m_shortopt_prefix << opt.shortopt << opt_sep;
            else 
                os.write(whitespace, min(m_shortopt_prefix.size()+1+opt_sep.size(), maxwhitespace));

            if (opt.longopt) {
                os << m_longopt_prefix << opt.longopt;

                string::size_type s = m_longopt_prefix.size() + strlen(opt.longopt);

                if (opt.argument_info) {
                    os << '=' << opt.argument_info;
                    s += strlen(opt.argument_info) + 1;
                }

                os.write(whitespace, min(max_longopt+pad-s, maxwhitespace));
            } else 
                os.write(whitespace, min(max_longopt+pad+opt_sep.size(), maxwhitespace));

            if (opt.info)
                os << opt.info;

            os << endl;
        }
    }

    ArgsImpl() 
        { }

    ArgsImpl(const Args::Table table[]) { 
        add_table(table);
    }
};


// 
// --- Public interface
//

Args::Args()
    : mP { new ArgsImpl }
{ }

Args::Args(const Args::Table table[])
    : mP { new ArgsImpl(table) }
{ }

Args::~Args()
{
    mP.reset();
}

void Args::add_table(const Table table[])
{
    mP->add_table(table);
}

int Args::parse(int argc, const char* const argv[], int pos)
{
    return mP->parse(argc, argv, pos);
}

string Args::program_name() const
{
    return mP->m_programname;
}

string Args::get(const string& name, const string& def) const
{
    return mP->get(name, def);
}

bool Args::is_set(const string& name) const
{
    return mP->is_set(name);
}

vector<string> Args::options() const
{
    return mP->options();
}

vector<string> Args::arguments() const
{
    return mP->arguments();
}

void Args::print_available_options(ostream& os) const
{
    mP->print_available_options(os);
}

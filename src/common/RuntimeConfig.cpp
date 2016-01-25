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

/// @file RuntimeConfig.cpp
/// RuntimeConfig class implementation

#include "RuntimeConfig.h"

#include "util/split.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <unordered_map>
#include <vector>

using namespace cali;
using namespace std;


namespace 
{
    const string prefix { "cali" };

    const char*  builtin_profiles =
        "# [serial-trace]\n"
        "CALI_SERVICES_ENABLE=event:recorder:timestamp:trace\n"
        "# [thread-trace]\n"
        "CALI_SERVICES_ENABLE=event:pthread:recorder:timestamp:trace\n"
        "# [mpi-trace]\n"
        "CALI_SERVICES_ENABLE=event:mpi:pthread:recorder:timestamp:trace\n"
        "# [load-sampling]\n"
        "CALI_SERVICES_ENABLE=event:pthread:recorder:timestamp:mitos\n"
        "CALI_LOG_VERBOSITY=2\n";

    string config_var_name(const string& name, const string& key) {
        // make uppercase PREFIX_NAMESPACE_KEY string

        string str;

        for ( string s : { prefix, string("_"), name, string("_"), key } )
            str.append(s);

        transform(str.begin(), str.end(), str.begin(), ::toupper);

        return str;
    }
}

namespace cali
{

//
// --- ConfigSet implementation
//

struct ConfigSetImpl
{
    // --- data

    unordered_map<string, ConfigSet::Entry> m_dict;

    // --- interface

    Variant get(const char* key) const {
        auto it = m_dict.find(key);
        return (it == m_dict.end() ? Variant() : Variant(string(it->second.value)));
    }

    void init(const char* name, const ConfigSet::Entry* list, const map<string, string>& profile) {
        for (const ConfigSet::Entry* e = list; e && e->key; ++e) {
            ConfigSet::Entry newent = *e;

            string varname = ::config_var_name(name, e->key);

            // See if there is an entry in the config profile

            auto it = profile.find(varname);
            if (it != profile.end())
                newent.value = it->second.c_str();

            // See if there is a config variable set

            char* val = getenv(::config_var_name(name, e->key).c_str());
            if (val)
                newent.value = val;

            m_dict.emplace(make_pair(string(e->key), newent));
        }
    }
};


//
// --- RuntimeConfig implementation
//

struct RuntimeConfigImpl
{
    // --- data

    static unique_ptr<RuntimeConfigImpl>     s_instance;
    static const ConfigSet::Entry            s_configdata[];

    ConfigSet                                m_config;

    string                                   m_profile_name;

    map< string, shared_ptr<ConfigSetImpl> > m_database;
    map< string, map<string, string> >       m_config_profiles;

    // --- helpers

    void read_config_profiles(istream& in) {
        //
        // Parse config file line-by-line
        // * '#' as the first character is a comment, or start of a new 
        //   group if a string enclosed in square brackets ("[group]") exists 
        // * Other lines are parsed as NAME=VALUE, or ignored if no '=' is found
        //

        map<string, string> current_profile;
        string              current_profile_name { "default" };

        for (string line; std::getline(in, line); ) {
            if (line.length() < 1)
                continue;

            if (line[0] == '#') {
                // is it a new profile?
                string::size_type b = line.find_first_of('[');
                string::size_type e = line.find_first_of(']');

                if (b != string::npos && e != string::npos && b+1 < e) {
                    if (current_profile.size() > 0) 
                        m_config_profiles[current_profile_name].insert(current_profile.begin(), current_profile.end());

                    current_profile.clear();
                    current_profile_name = line.substr(b+1, e-b-1);
                } else {
                    continue;
                }
            }

            string::size_type s = line.find_first_of('=');

            if (s > 0 && s < line.size())
                current_profile[line.substr(0, s)] = line.substr(s+1);
        }

        if (current_profile.size() > 0) 
            m_config_profiles[current_profile_name] = current_profile;
    }

    void read_config_files(const std::string& filenames) {
        // read builtin profiles

        istringstream is(::builtin_profiles);
        read_config_profiles(is);
        
        // read config files
        vector<string> files;

        util::split(filenames, ':', back_inserter(files));

        for (const auto &s : files) {
            ifstream fs(s.c_str());

            if (fs)
                read_config_profiles(fs);
        }
    }


    // --- interface

    Variant get(const char* set, const char* key) const {
        auto it = m_database.find(set);
        return (it == m_database.end() ? Variant() : it->second->get(key));
    }

    shared_ptr<ConfigSetImpl> init(const char* name, const ConfigSet::Entry* list) {
        auto it = m_database.find(name);

        if (it != m_database.end())
            return it->second;

        shared_ptr<ConfigSetImpl> ret { new ConfigSetImpl };

        ret->init(name, list, m_config_profiles[m_profile_name]);
        m_database.insert(it, make_pair(string(name), ret));

        return ret;
    }

    void print(ostream& os) const {
        for ( auto set : m_database )
            for ( auto entry : set.second->m_dict )
                os << "# " << entry.second.descr << " (" << cali_type2string(entry.second.type) << ")\n" 
                   << ::config_var_name(set.first, entry.first) << '=' << entry.second.value << endl;
    }

    RuntimeConfigImpl() 
        : m_profile_name { "default" } {
        // read pre-init config set to get config file name from env var
        ConfigSetImpl pre_init_config;

        pre_init_config.init("config", s_configdata, map<string, string>());

        // read config files
        read_config_files(pre_init_config.get("file").to_string());
    }

    static RuntimeConfigImpl* instance() {
        if (!s_instance) {
            s_instance.reset(new RuntimeConfigImpl);

            // read "config" config set again (profile may have been set in file)
            s_instance->m_config = RuntimeConfig::init("config", s_configdata);
            s_instance->m_profile_name = s_instance->m_config.get("profile").to_string();
        }

        return s_instance.get();
    }
};

unique_ptr<RuntimeConfigImpl> RuntimeConfigImpl::s_instance { nullptr };

const ConfigSet::Entry RuntimeConfigImpl::s_configdata[] = {
    { "profile", CALI_TYPE_STRING, "default",
      "Configuration profile",
      "Configuration profile" 
    },
    { "file", CALI_TYPE_STRING, "caliper.config",
      "List of configuration files",
      "Colon-serparated list of configuration files" 
    },
    ConfigSet::Terminator
};

} // namespace cali


//
// --- ConfigSet public interface 
// 

ConfigSet::ConfigSet(const shared_ptr<ConfigSetImpl>& p)
    : mP { p }
{ }

Variant 
ConfigSet::get(const char* key) const 
{
    if (!mP)
        return Variant();

    return mP->get(key);
}


//
// --- RuntimeConfig public interface
//

Variant
RuntimeConfig::get(const char* set, const char* key)
{
    return RuntimeConfigImpl::instance()->get(set, key);
}

ConfigSet 
RuntimeConfig::init(const char* name, const ConfigSet::Entry* list)
{
    return ConfigSet(RuntimeConfigImpl::instance()->init(name, list));
}

void
RuntimeConfig::print(ostream& os)
{
    RuntimeConfigImpl::instance()->print(os);
}
 

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

// RuntimeConfig class implementation

#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/util/split.hpp"

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
        "# [annotation-profile]\n"
        "CALI_SERVICES_ENABLE=aggregate:event:report:timestamp\n"
        "CALI_TIMER_SNAPSHOT_DURATION=true\n"
        "CALI_TIMER_INCLUSIVE_DURATION=false\n"
        "CALI_AGGREGATE_KEY=annotation:function:loop\n"
        "CALI_EVENT_TRIGGER=annotation:function:loop\n"
        "CALI_REPORT_CONFIG=select sum(sum#time.duration) group by annotation,function,loop format tree\n"
        "# [thread-trace]\n"
        "CALI_SERVICES_ENABLE=event:pthread:recorder:timestamp:trace\n"
        "# [mpi-trace]\n"
        "CALI_SERVICES_ENABLE=event:mpi:recorder:timestamp:trace\n"
        "# [load-sampling]\n"
        "CALI_SERVICES_ENABLE=event:recorder:timestamp:libpfm\n"
        "CALI_LOG_VERBOSITY=2\n";

    string config_var_name(const string& name, const string& key) {
        // make uppercase PREFIX_NAMESPACE_KEY string

        string str;

        for ( string s : { prefix, string("_"), name, string("_"), key } )
            str.append(s);

        transform(str.begin(), str.end(), str.begin(), ::toupper);

        return str;
    }

    typedef map< string, string > config_profile_t;
}

namespace cali
{

//
// --- ConfigSet implementation
//

struct ConfigSetImpl
{
    // --- data

    struct ConfigValue {
        ConfigSet::Entry entry;
        std::string      value;
    };

    unordered_map<string, ConfigValue> m_dict;

    // --- interface

    StringConverter get(const char* key) const {
        auto it = m_dict.find(key);
        return (it == m_dict.end() ? StringConverter() : StringConverter(it->second.value));
    }

    void init(const char* name, const ConfigSet::Entry* list, bool read_env, const ::config_profile_t& profile, const ::config_profile_t& top_profile)
    {
        for (const ConfigSet::Entry* e = list; e && e->key; ++e) {
            ConfigValue newent  { *e, string(e->value) };
            string      varname { ::config_var_name(name, e->key) };

            // See if there is an entry in the top config profile
            auto topit = top_profile.find(varname);

            if (topit != top_profile.end()) {
                newent.value = topit->second;
            } else {
                // See if there is an entry in the base config profile
                auto it = profile.find(varname);
                if (it != profile.end())
                    newent.value = it->second;
                
                if (read_env) {
                    // See if there is a config variable set
                    char* val = getenv(::config_var_name(name, e->key).c_str());
                    if (val)
                        newent.value = val;
                }
            }

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

    static bool                              s_allow_read_env;

    // combined profile: initially receives settings made through "add" API,
    // then merges all selected profiles in here
    ::config_profile_t                       m_combined_profile;
    
    // top-priority profile: receives all settings made through "set" API
    // that overwrite other settings
    ::config_profile_t                       m_top_profile;

    // the DB of initialized config sets
    map< string, shared_ptr<ConfigSetImpl> > m_database;

    // the config profile DB
    map< string, ::config_profile_t>         m_config_profiles;

    // --- helpers

    void read_config_profiles(istream& in) {
        //
        // Parse config file line-by-line
        // * '#' as the first character is a comment, or start of a new 
        //   group if a string enclosed in square brackets ("[group]") exists 
        // * Other lines are parsed as NAME=VALUE, or ignored if no '=' is found
        //

        ::config_profile_t current_profile;
        string             current_profile_name { "default" };

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

    void init_config_database() {
        // read pre-init config set to get config file name from env var
        ConfigSetImpl init_config_cfg;
        init_config_cfg.init("config", s_configdata, s_allow_read_env, m_combined_profile, m_top_profile);

        // read config files
        read_config_files(init_config_cfg.get("file").to_string());

        // merge "default" profile into combined profile
        {
            for ( auto &p : m_config_profiles["default"] )
                m_combined_profile[p.first] = p.second;
        }

        // read "config" config again: profile may have been set in the file
        shared_ptr<ConfigSetImpl> config_cfg { new ConfigSetImpl };
        config_cfg->init("config", s_configdata, s_allow_read_env, m_combined_profile, m_top_profile);

        m_database.insert(make_pair("config", config_cfg));

        // get the selected config profile names
        vector<string> profile_names;        
        util::split(config_cfg->get("profile").to_string(), ',',
                    std::back_inserter(profile_names));

        // merge all selected profiles
        for (const std::string& profile_name : profile_names) {
            auto it = m_config_profiles.find(profile_name);

            if (it == m_config_profiles.end()) {
                std::cerr << "caliper: error: config profile \"" << profile_name << "\" not defined." << std::endl;
                continue;
            }

            for (auto &p : it->second)
                m_combined_profile[p.first] = p.second;
        }            
    }

    // --- interface

    StringConverter get(const char* set, const char* key) {
        if (m_database.empty())
            init_config_database();
        
        auto it = m_database.find(set);
        return (it == m_database.end() ? StringConverter() : StringConverter(it->second->get(key)));
    }

    void preset(const char* key, const std::string& value) {
        m_combined_profile[key] = value;
    }

    void set(const char* key, const std::string& value) {
        m_top_profile[key] = value;
    }
    
    shared_ptr<ConfigSetImpl> init(const char* name, const ConfigSet::Entry* list) {
        if (m_database.empty())
            init_config_database();
        
        auto it = m_database.find(name);

        if (it != m_database.end())
            return it->second;

        shared_ptr<ConfigSetImpl> ret { new ConfigSetImpl };

        ret->init(name, list, s_allow_read_env, m_combined_profile, m_top_profile);
        m_database.insert(it, make_pair(string(name), ret));

        return ret;
    }

    void define_profile(const char* name, const char* keyvallist[][2]) {
        ::config_profile_t profile;
        
        for ( ; (*keyvallist)[0] && (*keyvallist)[1]; ++keyvallist)
            profile[string((*keyvallist)[0])] = string((*keyvallist)[1]);

        m_config_profiles[string(name)] = std::move(profile);
    }

    void print(std::ostream& os) const {
        for ( auto set : m_database )
            for ( auto entry : set.second->m_dict )
                os << "# " << entry.second.entry.descr
                   << " (" << cali_type2string(entry.second.entry.type) << ")\n" 
                   << ::config_var_name(set.first, entry.first)
                   << '=' << entry.second.value << std::endl;
    }

    static RuntimeConfigImpl* instance() {
        if (!s_instance)
            s_instance.reset(new RuntimeConfigImpl);

        return s_instance.get();
    }
};

unique_ptr<RuntimeConfigImpl> RuntimeConfigImpl::s_instance { nullptr };

const ConfigSet::Entry RuntimeConfigImpl::s_configdata[] = {
    { "profile",  CALI_TYPE_STRING, "default",
      "Configuration profile",
      "Configuration profile" 
    },
    { "file",     CALI_TYPE_STRING, "caliper.config",
      "List of configuration files",
      "Colon-serparated list of configuration files" 
    },
    ConfigSet::Terminator
};

bool RuntimeConfigImpl::s_allow_read_env { true };

} // namespace cali


//
// --- ConfigSet public interface 
// 

ConfigSet::ConfigSet(const shared_ptr<ConfigSetImpl>& p)
    : mP { p }
{ }

StringConverter
ConfigSet::get(const char* key) const 
{
    if (!mP)
        return std::string();

    return mP->get(key);
}


//
// --- RuntimeConfig public interface
//

StringConverter
RuntimeConfig::get(const char* set, const char* key)
{
    return RuntimeConfigImpl::instance()->get(set, key);
}

void
RuntimeConfig::preset(const char* key, const std::string& value)
{
    RuntimeConfigImpl::instance()->preset(key, value);
}

void
RuntimeConfig::set(const char* key, const std::string& value)
{
    RuntimeConfigImpl::instance()->set(key, value);
}

ConfigSet 
RuntimeConfig::init(const char* name, const ConfigSet::Entry* list)
{
    return ConfigSet(RuntimeConfigImpl::instance()->init(name, list));
}

void
RuntimeConfig::define_profile(const char* name,
                              const char* keyvallist[][2])
{
    RuntimeConfigImpl::instance()->define_profile(name, keyvallist);
}

void
RuntimeConfig::print(ostream& os)
{
    RuntimeConfigImpl::instance()->print(os);
}
 
bool
RuntimeConfig::allow_read_env()
{
    return RuntimeConfigImpl::s_allow_read_env;
}

bool
RuntimeConfig::allow_read_env(bool allow)
{
    RuntimeConfigImpl::s_allow_read_env = allow;
    return RuntimeConfigImpl::s_allow_read_env;
}

// "hidden" function to be used by tests

namespace cali
{

void
clear_caliper_runtime_config()
{
    RuntimeConfigImpl::s_instance.reset();
}

}

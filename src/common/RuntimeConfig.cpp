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

#include "util/parse_util.h"

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
        "CALI_SERVICES_ENABLE=event,recorder,timestamp,trace\n"
        "# [flat-function-profile]\n"
        "CALI_SERVICES_ENABLE=aggregate,event,report,timestamp\n"
        "CALI_AGGREGATE_KEY=event.end#function\n"
        "CALI_REPORT_CONFIG=\"select event.end#function,sum#time.inclusive.duration where event.end#function format table order by time.inclusive.duration desc\"\n"
        "# [runtime-report]\n"
        "CALI_SERVICES_ENABLE=aggregate,event,report,timestamp\n"
        "CALI_EVENT_ENABLE_SNAPSHOT_INFO=false\n"
        "CALI_TIMER_SNAPSHOT_DURATION=true\n"
        "CALI_TIMER_INCLUSIVE_DURATION=false\n"
        "CALI_REPORT_CONFIG=\"select inclusive_sum(sum#time.duration) as \\\"Inclusive time (usec)\\\",sum(sum#time.duration) as \\\"Exclusive time (usec)\\\",percent_total(sum#time.duration) as \\\"Time %\\\" group by prop:nested format tree\"\n"
        "CALI_REPORT_FILENAME=stderr\n"
        "# [mpi-runtime-report]\n"
        "CALI_SERVICES_ENABLE=aggregate,event,mpi,mpireport,timestamp\n"
        "CALI_MPI_BLACKLIST=MPI_Comm_rank,MPI_Comm_size,MPI_Wtick,MPI_Wtime\n"
        "CALI_EVENT_ENABLE_SNAPSHOT_INFO=false\n"
        "CALI_TIMER_SNAPSHOT_DURATION=true\n"
        "CALI_TIMER_INCLUSIVE_DURATION=false\n"
        "CALI_MPIREPORT_CONFIG=\"select min(sum#time.duration) as \\\"Min time/rank\\\",max(sum#time.duration) as \\\"Max time/rank\\\", avg(sum#time.duration) as \\\"Avg time/rank\\\", percent_total(sum#time.duration) as \\\"Time % (total)\\\" group by prop:nested format tree\"\n"
        "CALI_MPIREPORT_FILENAME=stderr\n"
        "# [thread-trace]\n"
        "CALI_SERVICES_ENABLE=event:pthread:recorder:timestamp:trace\n"
        "# [mpi-msg-trace]\n"
        "CALI_SERVICES_ENABLE=event,mpi,recorder,timestamp,trace\n"
        "CALI_MPI_BLACKLIST=MPI_Comm_rank,MPI_Comm_size,MPI_Wtick,MPI_Wtime\n"
        "CALI_MPI_MSG_TRACING=true\n"
        "CALI_TIMER_SNAPSHOT_DURATION=true\n"
        "CALI_TIMER_INCLUSIVE_DURATION=false\n"
        "CALI_TIMER_OFFSET=true\n"
        "CALI_RECORDER_FILENAME=%mpi.rank%.cali\n";

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

struct RuntimeConfig::RuntimeConfigImpl
{
    // --- data
    static unique_ptr<RuntimeConfig>         s_default_config;
    static const ConfigSet::Entry            s_configdata[];

    bool                                     m_allow_read_env = false;

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

            if (s > 0 && s < line.size()) {
                std::istringstream is(line.substr(s+1));
                current_profile[line.substr(0, s)] = util::read_word(is, "");
            }
        }

        if (current_profile.size() > 0)
            m_config_profiles[current_profile_name] = current_profile;
    }

    void read_config_files(const std::vector<std::string>& filenames) {
        // read builtin profiles

        istringstream is(::builtin_profiles);
        read_config_profiles(is);


        for (const auto &s : filenames) {
            ifstream fs(s.c_str());

            if (fs)
                read_config_profiles(fs);
        }
    }

    void init_config_database() {
        // read pre-init config set to get config file name from env var
        ConfigSetImpl init_config_cfg;
        init_config_cfg.init("config", s_configdata, m_allow_read_env, m_combined_profile, m_top_profile);

        // read config files
        read_config_files(init_config_cfg.get("file").to_stringlist());

        // merge "default" profile into combined profile
        {
            for ( auto &p : m_config_profiles["default"] )
                m_combined_profile[p.first] = p.second;
        }

        // read "config" config again: profile may have been set in the file
        shared_ptr<ConfigSetImpl> config_cfg { new ConfigSetImpl };
        config_cfg->init("config", s_configdata, m_allow_read_env, m_combined_profile, m_top_profile);

        m_database.insert(make_pair("config", config_cfg));

        // get the selected config profile names
        vector<string> profile_names =
            config_cfg->get("profile").to_stringlist();

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

    shared_ptr<ConfigSetImpl> init_configset(const char* name, const ConfigSet::Entry* list) {
        if (m_database.empty())
            init_config_database();

        auto it = m_database.find(name);

        if (it != m_database.end())
            return it->second;

        shared_ptr<ConfigSetImpl> ret { new ConfigSetImpl };

        ret->init(name, list, m_allow_read_env, m_combined_profile, m_top_profile);
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
};

unique_ptr<RuntimeConfig> RuntimeConfig::RuntimeConfigImpl::s_default_config { nullptr };

const ConfigSet::Entry RuntimeConfig::RuntimeConfigImpl::s_configdata[] = {
    { "profile",  CALI_TYPE_STRING, "default",
      "Configuration profile",
      "Configuration profile"
    },
    { "file",     CALI_TYPE_STRING, "caliper.config",
      "List of configuration files",
      "Comma-separated list of configuration files"
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

RuntimeConfig::RuntimeConfig()
    : mP(new RuntimeConfigImpl)
{ }

StringConverter
RuntimeConfig::get(const char* set, const char* key)
{
    return mP->get(set, key);
}

ConfigSet
RuntimeConfig::init_configset(const char* name, const ConfigSet::Entry* list)
{
    return ConfigSet(mP->init_configset(name, list));
}

void
RuntimeConfig::preset(const char* key, const std::string& value)
{
    mP->preset(key, value);
}

void
RuntimeConfig::set(const char* key, const std::string& value)
{
    mP->set(key, value);
}

void
RuntimeConfig::define_profile(const char* name,
                              const char* keyvallist[][2])
{
    mP->define_profile(name, keyvallist);
}

void
RuntimeConfig::print(ostream& os)
{
    mP->print(os);
}

bool
RuntimeConfig::allow_read_env()
{
    return mP->m_allow_read_env;
}

bool
RuntimeConfig::allow_read_env(bool allow)
{
    mP->m_allow_read_env = allow;
    return mP->m_allow_read_env;
}

//
// static interface
//

ConfigSet
RuntimeConfig::init(const char* name, const ConfigSet::Entry* list)
{
    return ConfigSet(get_default_config()->init_configset(name, list));
}

RuntimeConfig*
RuntimeConfig::get_default_config()
{
    if (!RuntimeConfigImpl::s_default_config) {
        RuntimeConfigImpl::s_default_config.reset(new RuntimeConfig);
        RuntimeConfigImpl::s_default_config->allow_read_env(true);
    }
    
    return RuntimeConfigImpl::s_default_config.get();
}

// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.h"

#include "caliper/ChannelController.h"

#include "caliper/common/Log.h"

#include "../src/common/util/parse_util.h"

#include <sstream>

using namespace cali;

namespace cali
{

// defined in controllers/controllers.cpp
extern ConfigManager::ConfigInfo builtin_controllers_table[];

}

namespace
{

struct ConfigInfoList {
    const ConfigManager::ConfigInfo* configs;
    ConfigInfoList* next;
};

ConfigInfoList  s_default_configs { cali::builtin_controllers_table, nullptr };
ConfigInfoList* s_config_list     { &s_default_configs };

}

struct ConfigManager::ConfigManagerImpl
{
    ChannelList m_channels;

    bool        m_error = false;
    std::string m_error_msg = "";

    argmap_t    m_default_parameters;

    void
    set_error(const std::string& msg) {
        m_error = true;
        m_error_msg = msg;
    }
    
    argmap_t
    parse_arglist(std::istream& is, const char* argtbl[]) {
        argmap_t args = m_default_parameters;

        char c = util::read_char(is);

        if (!is.good())
            return args;

        if (c != '(') {
            is.unget();
            return args;
        }

        do {
            std::string key = util::read_word(is, ",=()\n");

            while (*argtbl && key != std::string(*argtbl))
                ++argtbl;

            if (*argtbl == nullptr) {
                set_error("Unknown argument: " + key);
                args.clear();
                return args;
            }
            
            c = util::read_char(is);

            if (c != '=') {
                set_error("Expected '=' after " + key);
                args.clear();
                return args;
            }
            
            args[key] = util::read_word(is, ",=()\n");
            c = util::read_char(is);
        } while (is.good() && c == ',');

        if (c != ')') {
            set_error("Expected ')'");
            is.unget();
            args.clear();
        }

        return args;
    }

    std::vector< std::pair<const ConfigInfo*, argmap_t> >
    parse_configstring(const char* config_string) {
        std::vector< std::pair<const ConfigInfo*, argmap_t> > ret;
        
        std::istringstream is(config_string);
        char c = 0;

        do {
            std::string name = util::read_word(is, ",=()\n");

            const ::ConfigInfoList* lst_p = ::s_config_list;
            const ConfigInfo* cfg_p = nullptr;
            
            while (lst_p) {
                cfg_p = lst_p->configs;
            
                while (cfg_p && cfg_p->name && name != std::string(cfg_p->name))
                    ++cfg_p;

                if (cfg_p && cfg_p->name)
                    break;

                lst_p = lst_p->next;
            }
            
            if (!cfg_p || !cfg_p->name) {
                set_error("Unknown config: " + name);
                return ret;
            }

            auto args = parse_arglist(is, cfg_p->args);

            if (m_error)
                return ret;

            ret.push_back(std::make_pair(cfg_p, std::move(args)));

            c = util::read_char(is);
        } while (!m_error && is.good() && c == ',');
        
        return ret;
    }

    bool add(const char* config_string) {
        auto configs = parse_configstring(config_string);

        if (!m_error)
            for (auto cfg : configs)
                m_channels.emplace_back( (cfg.first->create)(std::move(cfg.second)) );

        return m_error;
    }

    ConfigManagerImpl()
        { }
};


ConfigManager::ConfigManager()
    : mP(new ConfigManagerImpl)
{ }

ConfigManager::ConfigManager(const char* config_string)
    : mP(new ConfigManagerImpl)
{
    mP->add(config_string);
}

ConfigManager::~ConfigManager()
{
    mP.reset();
}

bool
ConfigManager::add(const char* config_str)
{
    return mP->add(config_str);
}

bool
ConfigManager::error() const
{
    return mP->m_error;
}

std::string
ConfigManager::error_msg() const
{
    return mP->m_error_msg;
}

void
ConfigManager::set_default_parameter(const char* key, const char* value)
{
    mP->m_default_parameters[key] = value;
}

ConfigManager::ChannelList
ConfigManager::get_all_channels()
{
    return mP->m_channels;
}

ConfigManager::ChannelPtr
ConfigManager::get_channel(const char* name)
{
    for (ChannelPtr& chn : mP->m_channels)
        if (chn->name() == name)
            return chn;
    
    return ChannelPtr();
}

void
ConfigManager::add_controllers(const ConfigManager::ConfigInfo* ctrlrs)
{
    ::ConfigInfoList* elem = new ConfigInfoList { ctrlrs, ::s_config_list };
    s_config_list = elem;
}

std::vector<std::string>
ConfigManager::available_configs()
{
    std::vector<std::string> ret;
    
    for (const ConfigInfoList* lp = s_config_list; lp; lp = lp->next)
        for (const ConfigInfo* cp = lp->configs; cp && cp->name; ++cp)
            ret.push_back(cp->name);

    return ret;
}

std::vector<std::string>
ConfigManager::get_config_docstrings()
{
    std::vector<std::string> ret;

    for (const ConfigInfoList* lp = s_config_list; lp; lp = lp->next)
        for (const ConfigInfo* cp = lp->configs; cp && cp->name; ++cp)
            ret.push_back(cp->description);

    return ret;
}

std::string
ConfigManager::check_config_string(const char* config_string)
{
    ConfigManagerImpl tmp;
    tmp.parse_configstring(config_string);

    return tmp.m_error_msg;
}

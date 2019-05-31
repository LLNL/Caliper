// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.hpp"

#include "RuntimeProfileController.hpp"

#include "../src/common/util/parse_util.h"

#include <sstream>

using namespace cali;

namespace
{

cali::ChannelController*
make_runtime_report_controller(const ConfigManager::argmap_t& args, bool use_mpi)
{
    auto it = args.find("output");
    std::string output = (it == args.end() ? "" : it->second);

    if (output.empty())
        output = "stderr";
    
    return new RuntimeProfileController(use_mpi, output.c_str());
}

const char* runtime_report_args[] = { "output", nullptr };

ConfigManager::ConfigInfo config_table[] = {
    { "runtime-report", runtime_report_args, make_runtime_report_controller },
    
    { nullptr, nullptr, nullptr }
};

struct ConfigInfoList {
    const ConfigManager::ConfigInfo* configs;
    ConfigInfoList* next;
};

ConfigInfoList  s_default_configs { config_table, nullptr };
ConfigInfoList* s_config_list     { &s_default_configs };

}

struct ConfigManager::ConfigManagerImpl
{
    ChannelList m_channels;

    bool        m_use_mpi;

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

            const char** arg = argtbl;

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
    
    bool add(const char* config_string) {
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
                return false;
            }

            auto args = parse_arglist(is, cfg_p->args);

            if (m_error)
                return false;
            
            m_channels.emplace_back( (*cfg_p->create)(args, m_use_mpi) );

            c = util::read_char(is);
        } while (!m_error && is.good() && c == ',');
        
        return !m_error;
    }

    ConfigManagerImpl()
        : m_use_mpi(false)
        {
#ifdef CALIPER_HAVE_MPI
            m_use_mpi = true;
#endif
        }
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
ConfigManager::use_mpi(bool enable)
{
#ifndef CALIPER_HAVE_MPI
    Log(0).stream() << "ConfigManager: Cannot enable MPI support in non-MPI Caliper build!" << std::endl;
#endif
    mP->m_use_mpi = enable;
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

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
extern const ConfigManager::ConfigInfo* builtin_controllers_table[];

}

namespace
{

template<typename K, typename V>
std::map<K, V>
merge_new_elements(std::map<K, V>& to, const std::map<K, V>& from) {
    for (auto &p : from) {
        auto it = to.lower_bound(p.first);

        if (it == to.end() || it->first != p.first)
            to.emplace_hint(it, p.first, p.second);
    }

    return to;
}

struct ConfigInfoList {
    const ConfigManager::ConfigInfo** configs;
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
    argmap_t    m_extra_vars;

    void
    set_error(const std::string& msg) {
        m_error = true;
        m_error_msg = msg;
    }

    //   Parse "=value"
    // Returns an empty string if there is no '=', otherwise the string after '='.
    // Sets error if there is a '=' but no word afterwards.
    std::string
    read_value(std::istream& is, const std::string& key) {
        std::string val;
        char c = util::read_char(is);

        if (c == '=') {
            val = util::read_word(is, ",=()\n");

            if (val.empty())
                set_error("Expected value after \"" + key + "=\"");
        }
        else
            is.unget();

        return val;
    }

    argmap_t
    parse_arglist(std::istream& is, const char* argtbl[]) {
        argmap_t args;

        char c = util::read_char(is);

        if (!is.good())
            return args;

        if (c != '(') {
            is.unget();
            return args;
        }

        do {
            std::string key = util::read_word(is, ",=()\n");

            const char** argptr = argtbl;
            while (*argptr && key != std::string(*argptr))
                ++argptr;

            if (*argptr == nullptr) {
                set_error("Unknown argument: " + key);
                args.clear();
                return args;
            }

            std::string val = read_value(is, key);

            if (m_error) {
                args.clear();
                return args;
            }

            if (val.empty())
                val = "true";

            args[key] = val;

            c = util::read_char(is);
        } while (is.good() && c == ',');

        if (c != ')') {
            set_error("Expected ')'");
            is.unget();
            args.clear();
        }

        return args;
    }

    // Return config info object with given name, or null if not found
    const ConfigInfo*
    find_config(const std::string& name) {
        for (const ::ConfigInfoList *i = ::s_config_list; i; i = i->next)
           for (const ConfigInfo **j = i->configs; *j; j++) {
               const ConfigInfo *cfg_p = *j;
               if (cfg_p->name && cfg_p->name == name)
                  return cfg_p;
           }
        return nullptr;
    }

    // Return true if key is an option in any config
    bool
    is_option(const std::string& key) {
        for (const ::ConfigInfoList *i = ::s_config_list; i; i = i->next)
           for (const ConfigInfo **j = i->configs; *j; j++) {
              const ConfigInfo *cfg_p = *j;
              for (const char **opt = cfg_p->args; opt && *opt; opt++)
                 if (key == std::string(*opt))
                    return true;
           }
        return false;
    }

    //   Returns found configs with their args. Also updates the default parameters list
    // and extra variables list.
    std::vector< std::pair<const ConfigInfo*, argmap_t> >
    parse_configstring(const char* config_string) {
        std::vector< std::pair<const ConfigInfo*, argmap_t> > ret;

        std::istringstream is(config_string);
        char c = 0;

        //   Return if string is only whitespace.
        // Prevents empty strings being marked as errors.
        do {
            c = util::read_char(is);
        } while (is.good() && isspace(c));

        if (is.good())
            is.unget();
        else
            return ret;

        do {
            std::string key = util::read_word(is, ",=()\n");

            const ConfigInfo* cfg_p = find_config(key);

            if (cfg_p) {
                auto args = parse_arglist(is, cfg_p->args);

                if (m_error)
                    return ret;

                ret.push_back(std::make_pair(cfg_p, std::move(args)));
            } else {
                std::string val = read_value(is, key);

                if (m_error)
                    return ret;

                if (is_option(key)) {
                    if (val.empty())
                        val = "true";

                    m_default_parameters[key] = val;
                }
                else
                    m_extra_vars[key] = val;
            }

            c = util::read_char(is);
        } while (!m_error && is.good() && c == ',');

        return ret;
    }

    bool add(const char* config_string) {
        auto configs = parse_configstring(config_string);

        if (m_error)
            return false;

        for (auto cfg : configs) {
            argmap_t args = merge_new_elements(cfg.second, m_default_parameters);

            if (cfg.first->check_args) {
                std::string err = (cfg.first->check_args)(args);

                if (!err.empty())
                    set_error(err);
            }

            if (m_error)
                return false;
            else
                m_channels.emplace_back( (cfg.first->create)(args) );
        }

        return !m_error;
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
    mP->add(config_str);

    if (!mP->m_extra_vars.empty())
        mP->set_error("Unknown config or parameter: " + mP->m_extra_vars.begin()->first);

    return !mP->m_error;
}

bool
ConfigManager::add(const char* config_string, argmap_t& extra_kv_pairs)
{
    mP->add(config_string);

    extra_kv_pairs.insert(mP->m_extra_vars.begin(), mP->m_extra_vars.end());

    return !mP->m_error;
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
ConfigManager::start()
{
    for (ChannelPtr& chn : mP->m_channels)
        chn->start();
}

void
ConfigManager::flush()
{
    for (ChannelPtr& chn : mP->m_channels)
        chn->flush();
}

void
ConfigManager::add_controllers(const ConfigManager::ConfigInfo** ctrlrs)
{
    ::ConfigInfoList* elem = new ConfigInfoList { ctrlrs, ::s_config_list };
    s_config_list = elem;
}

std::vector<std::string>
ConfigManager::available_configs()
{
    std::vector<std::string> ret;

    for (const ConfigInfoList* lp = s_config_list; lp; lp = lp->next)
        for (const ConfigInfo** cp = lp->configs; *cp && (*cp)->name; ++cp)
            ret.push_back((*cp)->name);

    return ret;
}

std::vector<std::string>
ConfigManager::get_config_docstrings()
{
    std::vector<std::string> ret;

    for (const ConfigInfoList* lp = s_config_list; lp; lp = lp->next)
        for (const ConfigInfo** cp = lp->configs; *cp && (*cp)->name; ++cp)
            ret.push_back((*cp)->description);

    return ret;
}

std::string
ConfigManager::check_config_string(const char* config_string, bool allow_extra_kv_pairs)
{
    ConfigManagerImpl mgr;
    auto configs = mgr.parse_configstring(config_string);

    for (auto cfg : configs) {
        argmap_t args = merge_new_elements(cfg.second, mgr.m_default_parameters);

        if (cfg.first->check_args) {
            std::string err = (cfg.first->check_args)(args);

            if (!err.empty())
                mgr.set_error(err);
        }
    }

    if (!allow_extra_kv_pairs && !mgr.m_extra_vars.empty())
        mgr.set_error("Unknown config or parameter: " + mgr.m_extra_vars.begin()->first);

    return mgr.m_error_msg;
}

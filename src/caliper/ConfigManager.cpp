// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.h"

#include "caliper/common/Log.h"

#include "../src/common/util/parse_util.h"
#include "../src/common/util/format_util.h"

#include "../services/Services.h"

#include <algorithm>
#include <sstream>

using namespace cali;

namespace cali
{

// defined in controllers/controllers.cpp
extern const ConfigManager::ConfigInfo* builtin_controllers_table[];
extern const char* builtin_option_specs;

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

std::vector<std::string>
to_stringlist(const std::vector<StringConverter>& list)
{
    std::vector<std::string> ret;
    ret.reserve(list.size());

    for (const StringConverter& sc : list)
        ret.push_back(sc.to_string());

    return ret;
}

std::string
join_stringlist(const std::vector<std::string>& list)
{
    std::string ret;
    int c = 0;

    for (const std::string& s : list) {
        if (c++ > 0)
            ret.append(",");
        ret.append(s);
    }

    return ret;
}

struct ConfigInfoList {
    const ConfigManager::ConfigInfo** configs;
    ConfigInfoList* next;
};

ConfigInfoList  s_default_configs { cali::builtin_controllers_table, nullptr };
ConfigInfoList* s_config_list     { &s_default_configs };

} // namespace [anonymous]


//
// --- OptionSpec
//

class ConfigManager::OptionSpec
{
    struct query_arg_t {
        std::vector< std::pair<std::string, std::string> > select;
        std::vector< std::string> groupby;
    };

    struct option_desc_t {
        std::string type;
        std::string description;

        std::vector<std::string> categories;
        std::vector<std::string> services;

        std::map<std::string, query_arg_t> query_args;
        std::map<std::string, std::string> extra_config_flags;
    };

    std::map<std::string, option_desc_t> data;


    void parse_select(const std::vector<StringConverter>& list, query_arg_t& qarg) {
        for (const StringConverter& sc : list) {
            std::map<std::string, StringConverter> dict = sc.rec_dict();
            qarg.select.push_back(std::make_pair(dict["expr"].to_string(), dict["as"].to_string()));
        }
    }

    void parse_query_args(const std::vector<StringConverter>& list, option_desc_t& opt) {
        for (const StringConverter& sc : list) {
            std::map<std::string, StringConverter> dict = sc.rec_dict();
            query_arg_t qarg;

            auto it = dict.find("group by");
            if (it != dict.end())
                qarg.groupby = ::to_stringlist(it->second.rec_list());

            it = dict.find("select");
            if (it != dict.end())
                parse_select(it->second.rec_list(), qarg);

            it = dict.find("level");
            if (it == dict.end()) {
                Log(0).stream() << "OptionSpec: query args: missing \"level\"" << std::endl;
                continue;
            }

            opt.query_args[it->second.to_string()] = qarg;
        }
    }

    void parse_config(const std::map<std::string, StringConverter>& dict, option_desc_t& opt) {
        for (auto p : dict)
            opt.extra_config_flags[p.first] = p.second.to_string();
    }

    void parse_spec(const std::map<std::string, StringConverter>& dict) {
        option_desc_t opt;
        bool ok = false;

        auto it = dict.find("categories");
        if (it != dict.end())
            opt.categories = ::to_stringlist(it->second.rec_list(&ok));
        it = dict.find("services");
        if (it != dict.end())
            opt.services = ::to_stringlist(it->second.rec_list(&ok));
        it = dict.find("extra_config_flags");
        if (it != dict.end())
            parse_config(it->second.rec_dict(&ok), opt);
        it = dict.find("query args");
        if (it != dict.end())
            parse_query_args(it->second.rec_list(&ok), opt);
        it = dict.find("type");
        if (it != dict.end())
            opt.type = it->second.to_string();
        it = dict.find("description");
        if (it != dict.end())
            opt.description = it->second.to_string();

        it = dict.find("name");
        if (it == dict.end()) {
            Log(0).stream() << "ConfigOptionManager: option description needs a name" << std::endl;
            return;
        }

        data[it->second.to_string()] = opt;
    }

public:

    OptionSpec()
    { }

    OptionSpec(const OptionSpec&) = default;
    OptionSpec(OptionSpec&&) = default;

    OptionSpec& operator = (const OptionSpec&) = default;
    OptionSpec& operator = (OptionSpec&&) = default;

    ~OptionSpec()
    { }

    void add(const OptionSpec& other) {
        data.insert(other.data.begin(), other.data.end());
    }

    void add(const char* txt) {
        if (!txt)
            return;

        bool ok = false;
        auto descriptions = StringConverter(std::string(txt)).rec_list(&ok);

        for (auto &p : descriptions) {
            if (!ok)
                break;

            parse_spec(p.rec_dict(&ok));
        }

        if (!ok)
            Log(0).stream() << "ConfigManager::OptionSpec::add(): parse error on "  
                            << util::clamp_string(txt, 32) 
                            << std::endl;
    }

    bool contains(const std::string& name) const {
        return data.find(name) != data.end();
    }

    std::map< std::string, std::string >
    get_option_descriptions() {
        std::map< std::string, std::string > ret;

        for (auto &p : data)
            ret.insert(std::make_pair(p.first, p.second.description));
        
        return ret;
    }

    friend class ConfigManager::Options;
};


//
// --- Options
//

struct ConfigManager::Options::OptionsImpl
{
    OptionSpec spec;
    argmap_t   args;

    std::vector<std::string> enabled_options;


    std::string
    check() const {
        // 
        // Check if option values have the correct datatype
        //
        for (const auto &arg : args) {
            auto it = spec.data.find(arg.first);

            if (it == spec.data.end())
                continue;
            
            if (it->second.type == "bool") {
                bool ok = false;
                StringConverter(arg.second).to_bool(&ok);
                if (!ok)
                    return std::string("Invalid value \"") 
                        + arg.second 
                        + std::string("\" for ")
                        + arg.first;
            }
        }

        //
        //   Check if the required services for all requested profiling options
        // are there
        //

        Services::add_default_services();
        std::vector<std::string> services = Services::get_available_services();

        for (const std::string &opt : enabled_options) {
            auto o_it = spec.data.find(opt);
            if (o_it == spec.data.end())
                continue;

            for (const std::string& required_service : o_it->second.services)
                if (std::find(services.begin(), services.end(), required_service) == services.end())
                    return required_service
                        + std::string(" required for ")
                        + o_it->first
                        + std::string(" option is not available");
        }

        return "";
    }

    void
    append_extra_config_flags(config_map_t& config) {
        for (const std::string &opt : enabled_options) {
            auto o_it = spec.data.find(opt);
            if (o_it == spec.data.end())
                continue;

            config.insert(o_it->second.extra_config_flags.begin(), o_it->second.extra_config_flags.end());
        }
    }

    std::string
    query_select(const std::string& level, const std::string& in) const {
        std::string ret = in;

        for (const std::string& opt : enabled_options) {
            auto s_it = spec.data.find(opt); // find option description
            if (s_it == spec.data.end())
                continue;

            auto l_it = s_it->second.query_args.find(level); // find query level
            if (l_it == s_it->second.query_args.end())
                continue;

            for (const auto &p : l_it->second.select) {
                if (p.first.empty())
                    break;
                if (!ret.empty())
                    ret.append(",");
                ret.append(p.first);
                if (!p.second.empty())
                    ret.append(" as \"").append(p.second).append("\"");
            }
        }

        return ret;
    }

    std::string
    query_groupby(const std::string& level, const std::string& in) const {
        std::string ret = in;

        for (const std::string& opt : enabled_options) {
            auto s_it = spec.data.find(opt); // find option description
            if (s_it == spec.data.end())
                continue;

            auto l_it = s_it->second.query_args.find(level); // find query level
            if (l_it == s_it->second.query_args.end())
                continue;

            for (const auto &s : l_it->second.groupby) {
                if (!ret.empty())
                    ret.append(",");
                ret.append(s);
            }
        }

        return ret;
    }

    void
    find_enabled_options() {
        for (const auto &argp : args) {
            auto s_it = spec.data.find(argp.first);

            if (s_it == spec.data.end())
                continue;

            //   Non-boolean options are enabled if they are present in args.
            // For boolean options, check if they are set to false or true.
            bool enabled = true;

            if (s_it->second.type == "bool")
                enabled = StringConverter(argp.second).to_bool();
            if (enabled)
                enabled_options.push_back(argp.first);
        }
    }

    OptionsImpl(const OptionSpec& s, const argmap_t& a)
        : spec(s), args(a)
        {
            find_enabled_options();
        }
};

ConfigManager::Options::Options(const OptionSpec& spec, const argmap_t& args)
    : mP(new OptionsImpl(spec, args))
{ }

ConfigManager::Options::~Options()
{
    mP.reset();
}

bool
ConfigManager::Options::is_set(const char* option) const
{
    return mP->args.find(option) != mP->args.end();
}

bool
ConfigManager::Options::is_enabled(const char* option) const
{
    return std::find(mP->enabled_options.begin(), mP->enabled_options.end(),
                     std::string(option)) != mP->enabled_options.end();
}

StringConverter
ConfigManager::Options::get(const char* option, const char* default_val) const
{
    auto it = mP->args.find(option);

    if (it != mP->args.end())
        return StringConverter(it->second);

    return StringConverter(default_val);
}

std::string
ConfigManager::Options::check() const
{
    return mP->check();
}

std::string
ConfigManager::Options::services(const std::string& in) const
{
    std::vector<std::string> vec = StringConverter(in).to_stringlist();

    for (const std::string& opt : mP->enabled_options) {
        auto it = mP->spec.data.find(opt);

        if (it == mP->spec.data.end())
            continue;

        vec.insert(vec.end(), it->second.services.begin(), it->second.services.end());
    }

    // remove potential duplicates
    std::sort(vec.begin(), vec.end());
    auto last = std::unique(vec.begin(), vec.end());
    vec.erase(last, vec.end());

    return ::join_stringlist(vec);
}

void
ConfigManager::Options::append_extra_config_flags(config_map_t& config) const
{
    mP->append_extra_config_flags(config);
}

std::string
ConfigManager::Options::query_select(const std::string& level, const std::string& in) const
{
    return mP->query_select(level, in);
}

std::string
ConfigManager::Options::query_groupby(const std::string& level, const std::string& in) const
{
    return mP->query_groupby(level, in);
}


//
// --- ConfigManager
//

struct ConfigManager::ConfigManagerImpl
{
    ChannelList m_channels;

    bool        m_error = false;
    std::string m_error_msg = "";

    argmap_t    m_default_parameters;
    argmap_t    m_extra_vars;

    struct ConfigSpec {
        const ConfigInfo* info;
        OptionSpec opts;
    };

    std::map< std::string, std::shared_ptr<ConfigSpec> >
        m_spec;

    void
    set_error(const std::string& msg) {
        m_error = true;
        m_error_msg = msg;
    }

    void
    update_spec() {
        OptionSpec builtin_opts;
        builtin_opts.add(builtin_option_specs);

        for (const ::ConfigInfoList *i = ::s_config_list; i; i = i->next)
            for (const ConfigInfo **j = i->configs; *j; j++) {
                ConfigSpec spec;

                spec.info = *j;
                spec.opts.add(spec.info->options);
                spec.opts.add(builtin_opts);

                m_spec.emplace(spec.info->name, std::make_shared<ConfigSpec>(spec));
            }
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
    parse_arglist(std::istream& is, const OptionSpec& opts) {
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

            if (!opts.contains(key)) {
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

    // Return true if key is an option in any config
    bool
    is_option(const std::string& key) {
        for (const auto& p : m_spec)
            if (p.second->opts.contains(key))
                return true;

        return false;
    }

    //   Returns found configs with their args. Also updates the default parameters list
    // and extra variables list.
    std::vector< std::pair<const std::shared_ptr<ConfigSpec>, argmap_t> >
    parse_configstring(const char* config_string) {
        std::vector< std::pair<const std::shared_ptr<ConfigSpec>, argmap_t> > ret;

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

            auto spec_p = m_spec.find(key);
            if (spec_p != m_spec.end() && spec_p->second) {
                auto args = parse_arglist(is, spec_p->second->opts);

                if (m_error)
                    return ret;

                ret.push_back(std::make_pair(spec_p->second, std::move(args)));
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
            Options opts(cfg.first->opts, merge_new_elements(cfg.second, m_default_parameters));

            if (cfg.first->info->check_args) {
                std::string err = (cfg.first->info->check_args)(opts);

                if (!err.empty())
                    set_error(err);
            }

            if (m_error)
                return false;
            else
                m_channels.emplace_back( (cfg.first->info->create)(opts) );
        }

        return !m_error;
    }

    ConfigManagerImpl()
        {
            update_spec();
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
ConfigManager::stop()
{
    for (ChannelPtr& chn : mP->m_channels)
        chn->stop();
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

void
ConfigManager::cleanup()
{
    ::ConfigInfoList* ptr = s_config_list;

    while (ptr && ptr != &s_default_configs) {
        ::ConfigInfoList* tmp = ptr->next;
        delete ptr;
        ptr = tmp;
    }

    s_config_list = &s_default_configs;
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
        Options opts(cfg.first->opts, merge_new_elements(cfg.second, mgr.m_default_parameters));

        if (cfg.first->info->check_args) {
            std::string err = (cfg.first->info->check_args)(opts);

            if (!err.empty()) {
                mgr.set_error(err);
                break;
            }
        }

        std::string err = opts.check();

        if (!err.empty()) {
            mgr.set_error(std::string(cfg.first->info->name) + ": " + err);
            break;
        }
    }

    if (!allow_extra_kv_pairs && !mgr.m_extra_vars.empty())
        mgr.set_error("Unknown config or parameter: " + mgr.m_extra_vars.begin()->first);

    return mgr.m_error_msg;
}

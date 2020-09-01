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

extern void add_submodule_controllers_and_services();

}

namespace
{

ChannelController* make_basic_channel_controller(const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts)
{
    class BasicController : public ChannelController
    {
    public:
        BasicController(const char* name, const config_map_t& initial_cfg, const ConfigManager::Options& opts)
            : ChannelController(name, 0, initial_cfg)
            {
                // Hacky way to handle "output" option
                if (opts.is_set("output")) {
                    std::string output = opts.get("output").to_string();

                    config()["CALI_RECORDER_FILENAME" ] = output;
                    config()["CALI_REPORT_FILENAME"   ] = output;
                    config()["CALI_MPIREPORT_FILENAME"] = output;
                }

                opts.update_channel_config(config());
            }
    };

    return new BasicController(name, initial_cfg, opts);
}

class ConfigSpecManager
{
    std::vector<ConfigManager::ConfigInfo> m_configs;

public:

    void add_controller_specs(const ConfigManager::ConfigInfo** specs) {
        for (const ConfigManager::ConfigInfo** s = specs; s && *s; s++)
            m_configs.push_back(**s);
    }

    std::vector<ConfigManager::ConfigInfo> get_config_specs() const {
        return m_configs;
    }

    static ConfigSpecManager* instance() {
        static std::unique_ptr<ConfigSpecManager> mP { new ConfigSpecManager };

        if (mP->m_configs.empty())
            mP->add_controller_specs(builtin_controllers_table);

        return mP.get();
    }
};


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

} // namespace [anonymous]


//
// --- OptionSpec
//

class ConfigManager::OptionSpec
{
    struct select_expr_t {
        std::string expression;
        std::string alias;
        std::string unit;
    };

    struct query_arg_t {
        std::vector< select_expr_t > select;
        std::vector< std::string   > groupby;
        std::vector< std::string   > let;
    };

    struct option_spec_t {
        std::string type;
        std::string description;
        std::string category;

        std::vector<std::string> services;
        std::vector<std::string> inherited_specs;

        std::map<std::string, query_arg_t> query_args;
        std::map<std::string, std::string> config;
    };

    std::map<std::string, option_spec_t> data;

    bool        m_error;
    std::string m_error_msg;

    void set_error(const std::string& msg) {
        m_error = true;
        m_error_msg = msg;
    }

    void parse_select(const std::vector<StringConverter>& list, query_arg_t& qarg) {
        for (const StringConverter& sc : list) {
            std::map<std::string, StringConverter> dict = sc.rec_dict();
            qarg.select.push_back( {
                    dict["expr"].to_string(),
                    dict["as"  ].to_string(),
                    dict["unit"].to_string()
                } );
        }
    }

    void parse_query_args(const std::vector<StringConverter>& list, option_spec_t& opt) {
        for (const StringConverter& sc : list) {
            std::map<std::string, StringConverter> dict = sc.rec_dict();
            query_arg_t qarg;

            auto it = dict.find("group by");
            if (it != dict.end())
                qarg.groupby = ::to_stringlist(it->second.rec_list());

            it = dict.find("let");
            if (it != dict.end())
                qarg.let = ::to_stringlist(it->second.rec_list());

            it = dict.find("select");
            if (it != dict.end())
                parse_select(it->second.rec_list(), qarg);

            it = dict.find("level");
            if (it == dict.end()) {
                set_error(": query arg: missing \"level\"");
                continue;
            }

            opt.query_args[it->second.to_string()] = qarg;
        }
    }

    void parse_config(const std::map<std::string, StringConverter>& dict, option_spec_t& opt) {
        for (auto p : dict)
            opt.config[p.first] = p.second.to_string();
    }

    void parse_spec(const std::map<std::string, StringConverter>& dict) {
        option_spec_t opt;
        bool ok = true;

        auto it = dict.find("category");
        if (it != dict.end())
            opt.category = it->second.to_string();
        it = dict.find("services");
        if (it != dict.end())
            opt.services = ::to_stringlist(it->second.rec_list(&ok));
        it = dict.find("inherit");
        if (ok && !m_error && it != dict.end())
            opt.inherited_specs = ::to_stringlist(it->second.rec_list(&ok));
        it = dict.find("config");
        if (ok && !m_error && it != dict.end())
            parse_config(it->second.rec_dict(&ok), opt);
        it = dict.find("query args");
        if (ok && !m_error && it != dict.end())
            parse_query_args(it->second.rec_list(&ok), opt);
        it = dict.find("type");
        if (ok && !m_error && it != dict.end())
            opt.type = it->second.to_string();
        it = dict.find("description");
        if (ok && !m_error && it != dict.end())
            opt.description = it->second.to_string();

        it = dict.find("name");
        if (it == dict.end()) {
            set_error(": \"name\" missing");
            return;
        }

        if (!ok)
            set_error(": parse error");
        if (!m_error)
            data[it->second.to_string()] = opt;
    }

    std::vector<std::string> recursive_get_services_list(const std::string& cfg) {
        std::vector<std::string> ret;

        auto it = data.find(cfg);

        if (it == data.end())
            return ret;

        ret = it->second.services;

        for (const std::string& s : it->second.inherited_specs) {
            auto tmp = recursive_get_services_list(s);
            ret.insert(ret.end(), tmp.begin(), tmp.end());
        }

        std::sort(ret.begin(), ret.end());
        ret.erase(std::unique(ret.begin(), ret.end()), ret.end());

        return ret;
    }

public:

    OptionSpec()
        : m_error(false)
    { }

    OptionSpec(const OptionSpec&) = default;
    OptionSpec(OptionSpec&&) = default;

    OptionSpec& operator = (const OptionSpec&) = default;
    OptionSpec& operator = (OptionSpec&&) = default;

    ~OptionSpec()
    { }

    bool error() const {
        return m_error;
    }

    std::string error_msg() const {
        return m_error_msg;
    }

    void add(const OptionSpec& other, const std::vector<std::string>& categories) {
        for (const auto &p : other.data)
            if (std::find(categories.begin(), categories.end(), p.second.category) != categories.end())
                data.insert(p);
    }

    void add(const std::vector<StringConverter>& list) {
        if (m_error)
            return;

        for (auto &p : list) {
            parse_spec(p.rec_dict());

            if (m_error) {
                m_error_msg = std::string("option spec: ")
                    + util::clamp_string(p.to_string(), 32)
                    + m_error_msg;
                break;
            }
        }
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

    // throw out options requiring unavailable services
    void filter_unavailable_options(const std::vector<std::string>& in) {
        std::vector<std::string> available(in);
        std::sort(available.begin(), available.end());
        available.erase(std::unique(available.begin(), available.end()), available.end());

        auto it = data.begin();

        while (it != data.end()) {
            auto required = recursive_get_services_list(it->first);

            if (std::includes(available.begin(), available.end(), required.begin(), required.end()))
                ++it;
            else
                it = data.erase(it);
        }
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

        add_submodule_controllers_and_services();
        auto slist = services::get_available_services();

        for (const std::string &opt : enabled_options) {
            auto o_it = spec.data.find(opt);
            if (o_it == spec.data.end())
                continue;

            for (const std::string& required_service : o_it->second.services)
                if (std::find(slist.begin(), slist.end(), required_service) == slist.end())
                    return required_service
                        + std::string(" service required for ")
                        + o_it->first
                        + std::string(" option is not available");
        }

        return "";
    }

    std::string
    services(const std::string& in) {
        std::vector<std::string> vec = StringConverter(in).to_stringlist();

        for (const std::string& opt : enabled_options) {
            auto it = spec.data.find(opt);
            if (it == spec.data.end())
                continue;

            vec.insert(vec.end(), it->second.services.begin(), it->second.services.end());
        }

        // remove duplicates
        std::sort(vec.begin(), vec.end());
        auto last = std::unique(vec.begin(), vec.end());
        vec.erase(last, vec.end());

        return ::join_stringlist(vec);
    }

    void
    append_config(config_map_t& config) {
        for (const std::string &opt : enabled_options) {
            auto o_it = spec.data.find(opt);
            if (o_it == spec.data.end())
                continue;

            config.insert(o_it->second.config.begin(), o_it->second.config.end());
        }
    }

    void
    update_channel_config(config_map_t& config) {
        config["CALI_SERVICES_ENABLE"] = services(config["CALI_SERVICES_ENABLE"]);
        append_config(config);
    }

    std::string
    query_select(const std::string& level, const std::string& in, bool use_alias) const {
        std::string ret = in;

        for (const std::string& opt : enabled_options) {
            auto s_it = spec.data.find(opt); // find option description
            if (s_it == spec.data.end())
                continue;

            auto l_it = s_it->second.query_args.find(level); // find query level
            if (l_it == s_it->second.query_args.end())
                continue;

            for (const auto &p : l_it->second.select) {
                if (p.expression.empty())
                    break;

                if (!ret.empty())
                    ret.append(",");

                ret.append(p.expression);

                if (use_alias) {
                    if (!p.alias.empty())
                        ret.append(" as \"").append(p.alias).append("\"");
                    if (!p.unit.empty())
                        ret.append(" unit \"").append(p.unit).append("\"");
                }
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

    std::string
    query_let(const std::string& level, const std::string& in) const {
        std::string ret = in;

        for (const std::string& opt : enabled_options) {
            auto s_it = spec.data.find(opt); // find option description
            if (s_it == spec.data.end())
                continue;

            auto l_it = s_it->second.query_args.find(level); // find query level
            if (l_it == s_it->second.query_args.end())
                continue;

            for (const auto &s : l_it->second.let) {
                if (!ret.empty())
                    ret.append(",");
                ret.append(s);
            }
        }

        if (!ret.empty())
            ret = std::string(" let ") + ret;

        return ret;
    }

    std::vector<std::string>
    get_inherited_specs(const std::string& name) {
        std::vector<std::string> ret;

        auto it = spec.data.find(name);
        if (it == spec.data.end())
            return ret;

        for (const std::string& inh : it->second.inherited_specs) {
            auto tmp = get_inherited_specs(inh);
            ret.insert(ret.end(), tmp.begin(), tmp.end());
            ret.push_back(inh);
        }

        return ret;
    }

    void
    find_enabled_options() {
        std::vector<std::string> vec;

        {
            // hack: explicitly add options for deprecated "profile" argument

            auto it = args.find("profile");

            if (it != args.end()) {
                const struct profile_map_t {
                    const char* oldarg; const char* newarg;
                } profile_map[] = {
                    { "mpi",  "profile.mpi"  },
                    { "cuda", "profile.cuda" },
                    { "mem.highwatermark", "mem.highwatermark" }
                };

                auto pl = StringConverter(it->second).to_stringlist();

                for (const auto &p : profile_map)
                    if (std::find(pl.begin(), pl.end(), std::string(p.oldarg)) != pl.end())
                        vec.push_back(p.newarg);
            }
        }

        for (const auto &argp : args) {
            auto s_it = spec.data.find(argp.first);

            if (s_it == spec.data.end())
                continue;

            //   Non-boolean options are enabled if they are present in args.
            // For boolean options, check if they are set to false or true.
            bool enabled = true;

            if (s_it->second.type == "bool")
                enabled = StringConverter(argp.second).to_bool();
            if (enabled) {
                vec.push_back(argp.first);

                auto tmp = get_inherited_specs(argp.first);
                vec.insert(vec.end(), tmp.begin(), tmp.end());
            }
        }

        std::sort(vec.begin(), vec.end());
        vec.erase(std::unique(vec.begin(), vec.end()), vec.end());

        enabled_options = std::move(vec);
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

std::vector<std::string>
ConfigManager::Options::enabled_options() const
{
    std::vector<std::string> ret;

    for (const auto& s : mP->enabled_options) {
        auto s_it = mP->spec.data.find(s);
        if (s_it == mP->spec.data.end())
            continue;
        if (s_it->second.type == "bool")
            ret.push_back(s);
    }

    return ret;
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

void
ConfigManager::Options::update_channel_config(config_map_t& config) const
{
    mP->update_channel_config(config);
}

std::string
ConfigManager::Options::query_select(const char* level, const std::string& in, bool use_alias) const
{
    return mP->query_select(level, in, use_alias);
}

std::string
ConfigManager::Options::query_groupby(const char* level, const std::string& in) const
{
    return mP->query_groupby(level, in);
}

std::string
ConfigManager::Options::query_let(const char* level, const std::string& in) const
{
    return mP->query_let(level, in);
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

    OptionSpec  m_global_opts;

    struct config_spec_t {
        std::string    json; // the json spec
        CreateConfigFn create;
        CheckArgsFn    check_args;
        std::string    name;
        std::vector<std::string> categories;
        std::string    description;
        config_map_t   initial_cfg;
        OptionSpec     opts;
    };

    std::map< std::string, std::shared_ptr<config_spec_t> >
        m_spec;

    void
    set_error(const std::string& msg) {
        m_error = true;
        m_error_msg = msg;
    }

    // sets error if err is set
    void
    check_error(const std::string& err) {
        if (err.empty())
            return;

        set_error(err);
    }

    void
    add_option_spec(const char* jsonspec) {

    }

    void
    add_config_spec(const char* jsonspec, CreateConfigFn create, CheckArgsFn check, bool ignore_existing) {
        config_spec_t spec;
        bool ok = false;

        spec.json       = jsonspec;
        spec.create     = (create == nullptr) ? ::make_basic_channel_controller : create;
        spec.check_args = check;

        auto dict = StringConverter(spec.json).rec_dict(&ok);

        if (!ok) {
            set_error(std::string("spec parse error: ") + util::clamp_string(spec.json, 48));
            return;
        }

        auto it = dict.find("name");
        if (it == dict.end()) {
            set_error(std::string("'name' missing in spec: ") + util::clamp_string(spec.json, 48));
            return;
        }

        spec.name = it->second.to_string();

        // check if the spec already exists
        if (m_spec.count(spec.name) > 0) {
            if (!ignore_existing)
                set_error(spec.name + std::string(" already exists"));
            return;
        }

        std::vector<std::string> cfg_srvcs;

        it = dict.find("services");
        if (ok && !m_error && it != dict.end())
            cfg_srvcs = ::to_stringlist(it->second.rec_list(&ok));

        services::add_default_service_specs();
        auto slist = services::get_available_services();

        bool have_all_services = true;
        for (const auto& s : cfg_srvcs)
            if (std::find(slist.begin(), slist.end(), s) == slist.end())
                have_all_services = false;

        if (!have_all_services)
            return;

        it = dict.find("categories");
        if (ok && !m_error && it != dict.end())
            spec.categories = ::to_stringlist(it->second.rec_list(&ok));
        it = dict.find("description");
        if (ok && !m_error && it != dict.end())
            spec.description = it->second.to_string();
        it = dict.find("options");
        if (ok && !m_error && it != dict.end())
            spec.opts.add(it->second.rec_list(&ok));
        it = dict.find("config");
        if (ok && !m_error && it != dict.end())
            for (const auto &p : it->second.rec_dict(&ok))
                spec.initial_cfg[p.first] = p.second.to_string();

        if (cfg_srvcs.size() > 0)
            spec.initial_cfg["CALI_SERVICES_ENABLE"].append(::join_stringlist(cfg_srvcs));

        spec.opts.add(m_global_opts, spec.categories);
        spec.opts.filter_unavailable_options(slist);

        if (!ok)
            set_error(std::string("spec parse error: ") + util::clamp_string(spec.json, 48));
        if (!m_error)
            m_spec.emplace(spec.name, std::make_shared<config_spec_t>(spec));
    }

    void
    add_global_option_specs(const char* json) {
        bool ok = false;
        m_global_opts.add(StringConverter(json).rec_list(&ok));

        if (m_global_opts.error())
            set_error(m_global_opts.error_msg());
        if (!ok)
            set_error(std::string("parse error: ") + util::clamp_string(builtin_option_specs, 48));
    }

    void
    import_builtin_config_specs() {
        add_submodule_controllers_and_services();

        for (const ConfigInfo& s : ::ConfigSpecManager::instance()->get_config_specs())
            add_config_spec(s.spec, s.create, s.check_args, true /* ignore existing */ );
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

            if (!key.empty()) {
                if (!(key == "profile" || opts.contains(key))) {
                    set_error("Unknown option: " + key);
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
            }

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
        if (key == "profile") // special-case for deprecated "profile=" argument
            return true;

        for (const auto& p : m_spec)
            if (p.second->opts.contains(key))
                return true;

        return false;
    }

    //   Returns found configs with their args. Also updates the default parameters list
    // and extra variables list.
    std::vector< std::pair<const std::shared_ptr<config_spec_t>, argmap_t> >
    parse_configstring(const char* config_string) {
        import_builtin_config_specs();

        std::vector< std::pair<const std::shared_ptr<config_spec_t>, argmap_t> > ret;

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

            check_error(opts.check());

            if (cfg.first->check_args)
                check_error((cfg.first->check_args)(opts));

            if (m_error)
                return false;
            else
                m_channels.emplace_back( (cfg.first->create)(cfg.first->name.c_str(), cfg.first->initial_cfg, opts) );
        }

        return !m_error;
    }

    std::string
    get_documentation_for_spec(const char* name) const {
        std::string doc(name);

        auto it = m_spec.find(name);
        if (it == m_spec.end()) {
            doc.append(": Not available");
        } else {
            doc.append("\n ").append(it->second->description);

            auto optdescrmap = it->second->opts.get_option_descriptions();

            if (!optdescrmap.empty()) {
                doc.append("\n  Options:");
                for (const auto &op : optdescrmap)
                    doc.append("\n   ").append(op.first).append("\n    ").append(op.second);
            }
        }

        return doc;
    }

    std::vector<std::string>
    get_docstrings() const {
        std::vector<std::string> ret;

        for (const auto &p : m_spec) {
            std::string doc = p.first;
            doc.append("\n ").append(p.second->description);

            auto optdescrmap = p.second->opts.get_option_descriptions();

            if (!optdescrmap.empty()) {
                doc.append("\n  Options:");
                for (const auto &op : optdescrmap)
                    doc.append("\n   ").append(op.first).append("\n    ").append(op.second);
            }

            ret.push_back(doc);
        }

        return ret;
    }

    ConfigManagerImpl()
        {
            add_global_option_specs(builtin_option_specs);
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

void
ConfigManager::add_config_spec(const ConfigInfo& info)
{
    mP->add_config_spec(info.spec, info.create, info.check_args, false /* treat existing spec as error */);
}

void
ConfigManager::add_config_spec(const char* json)
{
    ConfigInfo info { json, nullptr, nullptr };
    add_config_spec(info);
}

void
ConfigManager::add_option_spec(const char* json)
{
    mP->add_global_option_specs(json);
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

std::string
ConfigManager::check(const char* configstr, bool allow_extra_kv_pairs) const
{
    // Make a copy of our data because parsing the string modifies its state
    ConfigManagerImpl tmpP(*mP);
    auto configs = tmpP.parse_configstring(configstr);

    for (auto cfg : configs) {
        Options opts(cfg.first->opts, merge_new_elements(cfg.second, tmpP.m_default_parameters));

        if (cfg.first->check_args)
            tmpP.check_error((cfg.first->check_args)(opts));

        tmpP.check_error(opts.check());

        if (tmpP.m_error)
            break;
    }

    if (!allow_extra_kv_pairs && !tmpP.m_extra_vars.empty())
        tmpP.set_error("Unknown config or parameter: " + tmpP.m_extra_vars.begin()->first);

    return tmpP.m_error_msg;
}

std::vector<std::string>
ConfigManager::available_config_specs() const
{
    mP->import_builtin_config_specs();

    std::vector<std::string> ret;
    for (const auto &p : mP->m_spec)
        ret.push_back(p.first);

    return ret;
}

std::string
ConfigManager::get_documentation_for_spec(const char* name) const
{
    mP->import_builtin_config_specs();

    return mP->get_documentation_for_spec(name);
}

std::vector<std::string>
ConfigManager::available_configs()
{
    return ConfigManager().available_config_specs();
}

std::vector<std::string>
ConfigManager::get_config_docstrings()
{
    ConfigManagerImpl mgr;
    mgr.import_builtin_config_specs();
    return mgr.get_docstrings();
}

std::string
ConfigManager::check_config_string(const char* configstr, bool allow_extra_kv_pairs)
{
    return ConfigManager().check(configstr, allow_extra_kv_pairs);
}

void
cali::add_global_config_specs(const ConfigManager::ConfigInfo** configs)
{
    ::ConfigSpecManager::instance()->add_controller_specs(configs);
}

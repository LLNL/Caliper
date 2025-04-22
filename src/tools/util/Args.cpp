// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

///@file Args.cpp
/// Argument parsing implementation

#include "Args.h"

#include <cstring>
#include <map>

using namespace cali::util;

struct Args::ArgsImpl {
    std::vector<Args::Table> m_options; ///< Option list

    std::map<std::string, std::vector<Table>::size_type> m_option_map; ///< Option key -> option index

    std::map<std::string, std::vector<Table>::size_type> m_long_options;  ///< Long option name -> option index
    std::map<char, std::vector<Table>::size_type>        m_short_options; ///< Short option name -> option index

    std::map<std::vector<Table>::size_type, std::string> m_active_options; ///< Option index -> option argument

    std::vector<std::string> m_arguments;   ///< Non-option arguments (filenames etc.)
    std::string              m_programname; ///< argv[0]

    bool m_fail; ///< Fail when parsing unknown option

    std::string m_shortopt_prefix { "-" };
    std::string m_longopt_prefix { "--" };
    std::string m_options_end { "--" };

    void add_table(const Table table[])
    {
        for (const Table* t = table; t->name; ++t) {
            auto n = m_options.size();

            m_options.push_back(*t);

            if (t->longopt)
                m_long_options.insert(std::make_pair(std::string(t->longopt), n));
            if (t->shortopt)
                m_short_options.insert(std::make_pair(t->shortopt, n));

            m_option_map.insert(std::make_pair(std::string(t->name), n));
        }
    }

    int parse(int argc, const char* const argv[], int pos)
    {
        if (argc <= pos)
            return pos;

        m_programname = argv[0];

        int i;

        for (i = pos; i < argc; ++i) {
            std::string arg { argv[i] };

            if (arg.empty())
                continue;
            if (arg == m_options_end)
                break;

            // --- handle long options

            auto lps = m_longopt_prefix.size();

            if (!m_longopt_prefix.empty() && arg.find(m_longopt_prefix) == 0) {
                std::string::size_type dpos = arg.find('=');

                auto it = m_long_options.find(arg.substr(lps, dpos == std::string::npos ? dpos : dpos - lps));

                if (it == m_long_options.end())
                    return i;

                std::string optarg;

                if (dpos != std::string::npos && dpos + 1 < arg.size())
                    optarg.assign(arg, dpos + 1, std::string::npos);
                if (m_options[it->second].has_argument && optarg.empty() && i + 1 < argc)
                    optarg.assign(argv[++i]);

                m_active_options.insert(make_pair(it->second, optarg));

                continue;
            }

            // handle short options

            auto sps = m_shortopt_prefix.size();

            if (!m_shortopt_prefix.empty() && arg.find(m_shortopt_prefix) == 0) {
                std::string::size_type dpos = arg.find('=');

                // handle characters in the shortopt string; last one may have an option argument
                for (auto n = sps; n < arg.size() && n < dpos; ++n) {
                    auto it = m_short_options.find(arg[n]);

                    if (it == m_short_options.end())
                        return i;

                    std::string optarg;

                    if (dpos + 1 < arg.size() && n == dpos - 1)
                        optarg.assign(arg, dpos + 1, std::string::npos);
                    if (n == arg.size() - 1 && m_options[it->second].has_argument && (i + 1) < argc)
                        optarg.assign(std::string(argv[++i]));

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

    std::string get(const std::string& name, const std::string& def) const
    {
        auto opt_it = m_option_map.find(name);

        if (opt_it == m_option_map.end())
            return def;

        auto it = m_active_options.find(opt_it->second);

        return it == m_active_options.end() ? def : it->second;
    }

    bool is_set(const std::string& name) const
    {
        auto opt_it = m_option_map.find(name);

        if (opt_it == m_option_map.end())
            return false;

        auto it = m_active_options.find(opt_it->second);

        return it != m_active_options.end();
    }

    std::vector<std::string> options() const
    {
        std::vector<std::string> out;

        for (auto const& p : m_active_options)
            out.emplace_back(m_options[p.first].name);

        return out;
    }

    std::vector<std::string> arguments() const { return m_arguments; }

    void print_available_options(std::ostream& os) const
    {
        const std::string::size_type pad         = 2;
        std::string::size_type       max_longopt = m_longopt_prefix.size();

        // Get max longopt+argument info size for padding
        for (auto const& l : m_long_options) {
            std::string::size_type s =
                m_longopt_prefix.size() + l.first.size()
                + (m_options[l.second].argument_info ? strlen(m_options[l.second].argument_info) + 1 : 0);

            max_longopt = std::max(max_longopt, s);
        }

        const std::string            opt_sep(", ");
        const char*             whitespace    = "                                        ";
        const std::string::size_type maxwhitespace = strlen(whitespace);

        for (const Table& opt : m_options) {
            os.write(whitespace, pad);

            if (opt.shortopt)
                os << m_shortopt_prefix << opt.shortopt << opt_sep;
            else
                os.write(whitespace, std::min(m_shortopt_prefix.size() + 1 + opt_sep.size(), maxwhitespace));

            if (opt.longopt) {
                os << m_longopt_prefix << opt.longopt;

                std::string::size_type s = m_longopt_prefix.size() + strlen(opt.longopt);

                if (opt.argument_info) {
                    os << '=' << opt.argument_info;
                    s += strlen(opt.argument_info) + 1;
                }

                os.write(whitespace, std::min(max_longopt + pad - s, maxwhitespace));
            } else
                os.write(whitespace, std::min(max_longopt + pad + opt_sep.size(), maxwhitespace));

            if (opt.info)
                os << opt.info;

            os << std::endl;
        }
    }

    ArgsImpl() {}

    ArgsImpl(const Args::Table table[]) { add_table(table); }
};

//
// --- Public interface
//

Args::Args() : mP { new ArgsImpl }
{}

Args::Args(const Args::Table table[]) : mP { new ArgsImpl(table) }
{}

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

std::string Args::program_name() const
{
    return mP->m_programname;
}

std::string Args::get(const std::string& name, const std::string& def) const
{
    return mP->get(name, def);
}

bool Args::is_set(const std::string& name) const
{
    return mP->is_set(name);
}

std::vector<std::string> Args::options() const
{
    return mP->options();
}

std::vector<std::string> Args::arguments() const
{
    return mP->arguments();
}

void Args::print_available_options(std::ostream& os) const
{
    mP->print_available_options(os);
}

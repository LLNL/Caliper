// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "query_common.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/tools-util/Args.h"

#include "caliper/common/Log.h"
#include "caliper/common/util/split.hpp"

#include "../../common/util/parse_util.h"

#include "../../services/Services.h"

#include "caliper/ConfigManager.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <sstream>

using namespace cali;
using namespace util;

namespace
{

/// \brief Parse "(arg1, arg2, ...)" argument list, ignoring whitespace
std::vector<std::string>
parse_arglist(std::istream& is)
{
    std::vector<std::string> ret;
    std::string word;

    char c = util::read_char(is);

    if (!is.good())
        return ret;

    if (c != '(') {
        is.unget();
        return ret;
    }

    do {
        std::string str = util::read_word(is, ",()");
        c = util::read_char(is);

        if (!str.empty() && (c == ',' || c == ')'))
            ret.push_back(str);
    } while (is.good() && c == ',');

    if (c != ')') {
        is.unget();
        ret.clear();
    }

    return ret;
}

std::pair< int, std::vector<std::string> >
parse_functioncall(std::istream& is, const QuerySpec::FunctionSignature* defs)
{
    // read function name
    std::string fname = util::read_word(is, ",()");

    if (fname.empty())
        return std::make_pair(-1, std::vector<std::string>());

    // find function among given signatures
    int retid = 0;

    for ( ; defs && defs[retid].name && (fname != defs[retid].name); ++retid)
        ;

    if (!defs || !defs[retid].name) {
        Log(0).stream() << "Unknown function \"" << fname << "\"" << std::endl;
        return std::make_pair(-1, std::vector<std::string>());
    }

    // read argument list
    std::vector<std::string> args = parse_arglist(is);
    int argsize = static_cast<int>(args.size());

    if (argsize < defs[retid].min_args || argsize > defs[retid].max_args) {
        Log(0).stream() << "Error: Expected " << defs[retid].min_args
                        << " arguments for function \"" << defs[retid].name
                        << "\" but got " << argsize << std::endl;

        return std::make_pair(-1, std::vector<std::string>());
    }

    return std::make_pair(retid, std::move(args));
}

}

namespace cali
{

bool
QueryArgsParser::parse_args(const Args& args)
{
    m_spec.filter.selection              = QuerySpec::FilterSelection::Default;
    m_spec.attribute_selection.selection = QuerySpec::AttributeSelection::Default;
    m_spec.aggregation_ops.selection     = QuerySpec::AggregationSelection::None;
    m_spec.aggregation_key.selection     = QuerySpec::AttributeSelection::None;
    m_spec.sort.selection                = QuerySpec::SortSelection::Default;
    m_spec.format.opt                    = QuerySpec::FormatSpec::Default;

    m_error = false;
    m_error_msg.clear();

    // parse CalQL query (if any)

    if (args.is_set("query")) {
        std::string q = args.get("query");
        CalQLParser p(q.c_str());

        if (p.error()) {
            m_error     = true;
            m_error_msg = p.error_msg();
            return false;
        } else
            m_spec = p.spec();
    }

    // setup filter

    if (args.is_set("select")) {
        m_spec.filter.selection = QuerySpec::FilterSelection::List;
        m_spec.filter.list      = RecordSelector::parse(args.get("select"));
    }

    // setup attribute selection

    if (args.is_set("attributes")) {
        m_spec.attribute_selection.selection = QuerySpec::AttributeSelection::List;
        util::split(args.get("attributes"), ',', std::back_inserter(m_spec.attribute_selection.list));
    }

    // setup aggregation

    if (args.is_set("aggregate")) {
        // aggregation ops
        m_spec.aggregation_ops.selection = QuerySpec::AggregationSelection::Default;

        std::string opstr = args.get("aggregate");

        if (!opstr.empty()) {
            m_spec.aggregation_ops.selection = QuerySpec::AggregationSelection::List;

            std::istringstream is(opstr);
            char c;

            const QuerySpec::FunctionSignature* defs = Aggregator::aggregation_defs();

            do {
                auto fpair = parse_functioncall(is, defs);

                if (fpair.first >= 0)
                    m_spec.aggregation_ops.list.emplace_back(defs[fpair.first], fpair.second);

                c = util::read_char(is);
            } while (is.good() && c == ',');
        }

        // aggregation key
        m_spec.aggregation_key.selection = QuerySpec::AttributeSelection::Default;

        if (args.is_set("aggregate-key")) {
            std::string keystr = args.get("aggregate-key");

            if (keystr == "none") {
                m_spec.aggregation_key.selection = QuerySpec::AttributeSelection::None;
            } else {
                m_spec.aggregation_key.selection = QuerySpec::AttributeSelection::List;
                util::split(keystr, ',', std::back_inserter(m_spec.aggregation_key.list));
            }
        }
    }

    // setup sort

    if (args.is_set("sort")) {
        m_spec.sort.selection = QuerySpec::SortSelection::List;

        std::vector<std::string> list;
        util::split(args.get("sort"), ',', std::back_inserter(list));

        for (const std::string& s : list)
            m_spec.sort.list.emplace_back(s);
    }

    // setup formatter

    for (const QuerySpec::FunctionSignature* fmtsig = FormatProcessor::formatter_defs(); fmtsig && fmtsig->name; ++fmtsig) {
        // see if a formatting option is set
        if (args.is_set(fmtsig->name)) {
            m_spec.format.opt       = QuerySpec::FormatSpec::User;
            m_spec.format.formatter = *fmtsig;

            // Find formatter args (if any)
            for (int i = 0; i < fmtsig->max_args; ++i)
                if (args.is_set(fmtsig->args[i])) {
                    m_spec.format.args.resize(i+1);
                    m_spec.format.args[i] = args.get(fmtsig->args[i]);
                }

            // NOTE: This check isn't complete yet.
            if (m_spec.format.args.size() < static_cast<std::size_t>(fmtsig->min_args)) {
                m_error     = true;
                m_error_msg = std::string("Insufficient arguments for formatter ") + fmtsig->name;

                return false;
            }

            break;
        }
    }

    return true;
}

void print_caliquery_help(const Args& args, const char* usage, const ConfigManager& mgr)
{
    std::string helpopt = args.get("help");

    if (helpopt == "configs") {
        auto list = mgr.available_config_specs();
        for (const auto &s : list)
            std::cout << mgr.get_documentation_for_spec(s.c_str()) << "\n";
    } else if (helpopt == "services") {
        services::add_default_service_specs();

        int i = 0;
        for (const auto& s : services::get_available_services())
            std::cout << (i++ > 0 ? "," : "") << s;
        std::cout << std::endl;
    } else if (!helpopt.empty()) {
        auto list = mgr.available_config_specs();

        if (std::find(list.begin(), list.end(), helpopt) == list.end()) {
            std::cerr << "Unknown help option \"" << helpopt << "\". Available options: "
                      << "\n  [none]:   Describe cali-query usage (default)"
                      << "\n  configs:  Describe all Caliper profiling configurations"
                      << "\n  [configname]: Describe profiling configuration [configname]"
                      << "\n  services: List available services"
                      << std::endl;
        } else {
            std::cout << mgr.get_documentation_for_spec(helpopt.c_str()) << std::endl;
        }
    } else {
        std::cout << usage << "\n\n";
        args.print_available_options(std::cout);
    }
}

}

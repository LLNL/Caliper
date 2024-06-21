// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "query_common.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/Preprocessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/tools-util/Args.h"

#include "caliper/common/Log.h"

#include "../../common/util/parse_util.h"
#include "../../common/util/format_util.h"
#include "../../common/util/split.hpp"

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

const char* s_calql_helpstr = R"helpstr(
The Caliper Query Language (CalQL) is used to to filter, aggregate, and create
reports from Caliper .cali data with cali-query.

The general structure of a query is:

LET
    <list of pre-processing operations>
SELECT
    <list of output attributes and aggregations>
GROUP BY
    <list of aggregation key attributes>
WHERE
    <list of conditions>
FORMAT
    <report/output formatter>
ORDER BY
    <list of sort attributes>

All of the statements are optional; by default cali-query will pass the input
records through as-is, without any aggregations, and output .cali data.
statements are case-insensitive and can be provided in any order.

Run "--help [let, select, groupby, where, format]" for more information about
each CalQL statement.
)helpstr";

const char* s_calql_let_helpstr = R"helpstr(
The LET statement defines operations to be applied on input records before
further processing. The general structure of the LET statement is

    LET result = op(arguments) [ IF condition ] [, ... ]

This adds a new attribute "result" with the result of operation "op" to
the record. Results are only added if the operation was successful
(e.g., all required input operands were present in the record). If an
optional IF condition is given, the operation is only applied if the
condition is true.

Available LET operators:

)helpstr";

const char* s_calql_select_helpstr = R"helpstr(
The SELECT statement selects the attributes and aggregations in the output.
The general structure is

    SELECT attribute | op(arguments) [ AS alias ] [ UNIT unit ] [, ...]

The aggregations in the SELECT statement specify how attributes are
aggregated. Use the GROUP BY statement to specify the output set. Use AS
to specify an optional custom label/header.

Available aggregation operations:

)helpstr";

const char* s_calql_groupby_helpstr = R"helpstr(
The GROUP BY statement selects the attributes that define the output set. For
example, when grouping by "mpi.rank", the output set has one record for each
mpi.rank value encountered in the input. Input records with the same mpi.rank
value will be aggregated as specified by the SELECT statement. The general
structure is

    GROUP BY path | attribute name [, ...]

The "path" value selects all region name attributes for grouping.
)helpstr";

const char* s_calql_where_helpstr = R"helpstr(
Use the WHERE statement to filter input records. The filter is applied after
pre-processing (see LET) and before aggregating. The general structure is

    WHERE [NOT] condition [, ...]

NOT negates the condition. Available conditions are:

  attribute         (matches if any entry for "attribute" is in the record)
  attribute = value
  attribute > value
  attribute < value
)helpstr";

const char* s_calql_format_helpstr = R"helpstr(
The FORMAT statement selects and configures the output formatter. The general
structure is

    FORMAT formatter [(arguments)] [ORDER BY attribute [ASC | DESC] [,...]]

The ORDER BY statement specifies a list of attributes to sort the output
records by. It can be used with the "table" and "tree" formatters.

Available formatters:

)helpstr";

std::ostream& print_function_signature(std::ostream& os, const QuerySpec::FunctionSignature& s)
{
    os << "  " << s.name << "(";
    for (int i = 0; i < s.min_args; ++i)
        os << (i > 0 ? ", " : "") << s.args[i];
    for (int i = s.min_args; i < s.max_args; ++i)
        os << (i > 0 ? ", " : "") << s.args[i] << "*";
    os << ")";
    return os;
}

}

namespace cali
{

bool
QueryArgsParser::parse_args(const Args& args)
{
    m_spec.filter.selection    = QuerySpec::FilterSelection::Default;
    m_spec.select.selection    = QuerySpec::AttributeSelection::Default;
    m_spec.aggregate.selection = QuerySpec::AggregationSelection::None;
    m_spec.groupby.selection   = QuerySpec::AttributeSelection::None;
    m_spec.sort.selection      = QuerySpec::SortSelection::Default;
    m_spec.format.opt          = QuerySpec::FormatSpec::Default;

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
    } else if (args.is_set("query-file")) {
        std::string filename = args.get("query-file");
        std::ifstream in { filename.c_str() };

        if (!in) {
            m_error = true;
            m_error_msg = "cannot open query file " + filename;
            return false;
        }

        CalQLParser p(in);

        if (p.error()) {
            m_error = true;
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
        m_spec.select.selection = QuerySpec::AttributeSelection::List;
        util::split(args.get("attributes"), ',', std::back_inserter(m_spec.select.list));
    }

    // setup aggregation

    if (args.is_set("aggregate")) {
        // aggregation ops
        m_spec.aggregate.selection = QuerySpec::AggregationSelection::Default;

        std::string opstr = args.get("aggregate");

        if (!opstr.empty()) {
            m_spec.aggregate.selection = QuerySpec::AggregationSelection::List;

            std::istringstream is(opstr);
            char c;

            const QuerySpec::FunctionSignature* defs = Aggregator::aggregation_defs();

            do {
                auto fpair = parse_functioncall(is, defs);

                if (fpair.first >= 0)
                    m_spec.aggregate.list.emplace_back(defs[fpair.first], fpair.second);

                c = util::read_char(is);
            } while (is.good() && c == ',');
        }

        // aggregation key
        m_spec.groupby.selection = QuerySpec::AttributeSelection::Default;

        if (args.is_set("aggregate-key")) {
            std::string keystr = args.get("aggregate-key");

            if (keystr == "none") {
                m_spec.groupby.selection = QuerySpec::AttributeSelection::None;
            } else {
                m_spec.groupby.selection = QuerySpec::AttributeSelection::List;
                util::split(keystr, ',', std::back_inserter(m_spec.groupby.list));
            }

            m_spec.groupby.use_path = false;

            auto it = std::find(m_spec.groupby.list.begin(), m_spec.groupby.list.end(),
                                "path");
            if (it != m_spec.groupby.list.end()) {
                m_spec.groupby.use_path = true;
                m_spec.groupby.list.erase(it);
            }
            it = std::find(m_spec.groupby.list.begin(), m_spec.groupby.list.end(),
                           "prop:nested");
            if (it != m_spec.groupby.list.end()) {
                m_spec.groupby.use_path = true;
                m_spec.groupby.list.erase(it);
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
                if (args.is_set(fmtsig->args[i]))
                    m_spec.format.kwargs[fmtsig->args[i]] = args.get(fmtsig->args[i]);

            // NOTE: This check isn't complete yet.
            if (m_spec.format.kwargs.size() < static_cast<std::size_t>(fmtsig->min_args)) {
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

    if (helpopt == "configs" || helpopt == "recipes") {
        std::cout << "Available config recipes:\n";
        auto list = mgr.available_config_specs();
        size_t len = 0;
        for (const auto &s : list)
            len = std::max(len, s.size());
        for (const auto &s : list) {
            std::string descr = mgr.get_description_for_spec(s.c_str());
            util::pad_right(std::cout << " ", s, len) << descr << "\n";
        }
    } else if (helpopt == "services") {
        std::cout << "Available services:\n";
        services::add_default_service_specs();
        auto list = services::get_available_services();
        size_t len = 0;
        for (const auto &s : list)
            len = std::max(len, s.size());
        for (const auto &s : list) {
            std::string descr = services::get_service_description(s);
            util::pad_right(std::cout << " ", s, len) << descr << "\n";
        }
    } else if (helpopt == "calql") {
        std::cout << s_calql_helpstr;
    } else if (helpopt == "let") {
        std::cout << s_calql_let_helpstr;
        const QuerySpec::FunctionSignature* ops = Preprocessor::preprocess_defs();
        for (const auto* p = ops; p && p->name; ++p)
            print_function_signature(std::cout, *p) << "\n";
    } else if (helpopt == "select") {
        std::cout << s_calql_select_helpstr;
        const QuerySpec::FunctionSignature* ops = Aggregator::aggregation_defs();
        for (const auto* p = ops; p && p->name; ++p) {
            print_function_signature(std::cout, *p) << " -> ";
            std::vector<std::string> args(p->args, p->args+p->max_args);
            const QuerySpec::AggregationOp op(*p, args);
            std::cout << Aggregator::get_aggregation_attribute_name(op) << "\n";
        }
    } else if (helpopt == "where") {
        std::cout << s_calql_where_helpstr;
    } else if (helpopt == "format") {
        std::cout << s_calql_format_helpstr;
        const QuerySpec::FunctionSignature* ops = FormatProcessor::formatter_defs();
        for (const auto* p = ops; p && p->name; ++p)
            print_function_signature(std::cout, *p) << "\n";
    } else if (!helpopt.empty()) {
        {
            auto cfgs = mgr.available_config_specs();
            auto it = std::find(cfgs.begin(), cfgs.end(), helpopt);
            if (it != cfgs.end()) {
                std::cout << mgr.get_documentation_for_spec(helpopt.c_str()) << "\n";
                return;
            }
        }

        {
            services::add_default_service_specs();
            auto srvs = services::get_available_services();
            auto it = std::find(srvs.begin(), srvs.end(), helpopt);
            if (it != srvs.end()) {
                services::print_service_documentation(std::cout << *it << " service:\n", helpopt);
                return;
            }
        }

        std::cerr << "Unknown help option \"" << helpopt << "\". Available options: "
                    << "\n  [none]:   Describe cali-query usage (default)"
                    << "\n  configs:  Describe all Caliper profiling configurations"
                    << "\n  [config or service name]: Describe profiling configuration or service"
                    << "\n  services: List available services"
                    << std::endl;
    } else {
        std::cout << usage << "\n\n";
        args.print_available_options(std::cout);
        std::cout <<
            "\n Use \"--help configs\" to list all config recipes."
            "\n Use \"--help services\" to list all available services."
            "\n Use \"--help [recipe name]\" to get help for a config recipe."
            "\n Use \"--help [service name]\" to get help for a service."
            "\n Use \"--help calql\" to get help for the CalQL query language."
            "\n Use \"--help [let,select,where,groupby,format]\" to get help for CalQL statements.\n";
    }
}

}

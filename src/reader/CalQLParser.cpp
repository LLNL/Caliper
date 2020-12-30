// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/reader/CalQLParser.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/Preprocessor.h"

#include "../common/util/parse_util.h"

#include <algorithm>
#include <cctype>
#include <sstream>

using namespace cali;

namespace
{

int
get_definition_id(const std::string& w, const QuerySpec::FunctionSignature* defs)
{
    if (!defs || w.empty())
        return -1;

    std::string wl(w);
    std::transform(w.begin(), w.end(), wl.begin(), ::tolower);

    int retid = 0;

    for ( ; defs[retid].name && wl != defs[retid].name; ++retid)
        ;

    return defs[retid].name ? retid : -1;
}

} // namespace [anonymous]


struct CalQLParser::CalQLParserImpl
{
    QuerySpec   spec;

    bool        error;
    std::string error_msg;
    std::istream::pos_type error_pos;

    enum Clause {
        None = 0,
        Aggregate,
        Format,
        Group,
        Select,
        Sort,
        Where,
        Let
    };

    Clause get_clause_from_word(const std::string& w) {
        const struct keyword_map_t {
            const char* name; Clause clause;
        } keywords[] = {
            { "aggregate", Aggregate },
            { "format",    Format    },
            { "group",     Group     },
            { "select",    Select    },
            { "order",     Sort      },
            { "where",     Where     },
            { "let",       Let       },

            { nullptr,     None      }
        };

        std::string wl(w);
        std::transform(w.begin(), w.end(), wl.begin(), ::tolower);

        for (const keyword_map_t* p = keywords; p->name; ++p)
            if (wl == p->name)
                return p->clause;

        return Clause::None;
    }

    void
    set_error(const std::string& s, std::istream& is) {
        error     = true;
        error_pos = is.tellg();
        error_msg = s;
    }

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
            std::string str = util::read_word(is, ",;=<>()\n");
            c = util::read_char(is);

            if (!str.empty() && (c == ',' || c == ')'))
                ret.push_back(str);
        } while (is.good() && c == ',');

        if (c != ')') {
            set_error("Expected ')'", is);
            is.unget();
            ret.clear();
        }

        return ret;
    }

    void
    parse_aggregate(std::istream& is) {
        const QuerySpec::FunctionSignature* defs = Aggregator::aggregation_defs();
        char c = '\0';

        do {
            std::string w = util::read_word(is, ",;=<>()\n");
            std::vector<std::string> args = parse_arglist(is);

            // check if this is an aggregation function

            int i = get_definition_id(w, defs);
            int argsize = static_cast<int>(args.size());

            if (i >= 0) {
                if (defs[i].min_args > argsize || defs[i].max_args < argsize) {
                    set_error(std::string("Invalid number of arguments for ") + defs[i].name, is);
                } else {
                    spec.aggregation_ops.selection = QuerySpec::AggregationSelection::List;
                    spec.aggregation_ops.list.emplace_back(defs[i], args);
                }
            } else {
                set_error(std::string("Unknown aggregation function ") + w, is);
            }

            c = util::read_char(is);
        } while (!error && is.good() && c == ',');

        if (c)
            is.unget();
    }

    void
    parse_format(std::istream& is) {
        const QuerySpec::FunctionSignature* defs = FormatProcessor::formatter_defs();

        std::string fname = util::read_word(is, ",;=<>()\n");
        std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);

        int i = 0;

        for ( ; defs[i].name && fname != defs[i].name; ++i)
            ;

        if (!defs[i].name) {
            set_error(std::string("Unknown formatter ")  + fname, is);
            return;
        }

        std::vector<std::string> args = parse_arglist(is);
        int argsize = static_cast<int>(args.size());

        if (defs[i].min_args > argsize || defs[i].max_args < argsize)
            set_error(std::string("Invalid number of arguments for formatter ") + fname, is);
        else {
            spec.format.opt       = QuerySpec::FormatSpec::User;
            spec.format.formatter = defs[i];
            spec.format.args      = args;
        }
    }

    void
    parse_groupby(std::istream& is) {
        char c = 0;

        do {
            std::string w = util::read_word(is, ",;=<>()\n");

            if (!w.empty()) {
                spec.aggregation_key.selection = QuerySpec::AttributeSelection::List;
                spec.aggregation_key.list.push_back(w);
            }

            c = util::read_char(is);
        } while (!error && is.good() && c == ',');

        if (c)
            is.unget();
    }

    void
    parse_select(std::istream& is) {
        // SELECT selection, selection, ...
        // selection : label [AS alias]
        //             | aggregation_function(parameters) [AS alias] [UNIT unit]
        //             | *

        const QuerySpec::FunctionSignature* defs = Aggregator::aggregation_defs();
        char c = '\0';

        std::string next_keyword;

        do {
            std::string selection_attr_name;

            c = util::read_char(is);

            if (c == '*') {
                spec.attribute_selection.selection = QuerySpec::AttributeSelection::All;
            } else {
                is.unget();

                std::string w = util::read_word(is, ",;=<>()\n");

                // check if this is an aggregation function

                char c = util::read_char(is);
                is.unget();

                if (c == '(') {
                    int i = get_definition_id(w, defs);

                    if (i >= 0) {
                        std::vector<std::string> args = parse_arglist(is);
                        int argsize = static_cast<int>(args.size());

                        if (defs[i].min_args > argsize || defs[i].max_args < argsize) {
                            set_error(std::string("Invalid number of arguments for ") + defs[i].name, is);
                        } else {
                            spec.aggregation_ops.selection = QuerySpec::AggregationSelection::List;

                            QuerySpec::AggregationOp op(defs[i], args);

                            spec.aggregation_ops.list.push_back(op);
                            selection_attr_name = Aggregator::get_aggregation_attribute_name(op);

                            // explicitly add aggregation attribute name to the list
                            if (spec.attribute_selection.selection != QuerySpec::AttributeSelection::All) {
                                spec.attribute_selection.selection = QuerySpec::AttributeSelection::List;
                                spec.attribute_selection.list.push_back(selection_attr_name);
                            }
                        }
                    } else {
                        set_error(std::string("Unknown aggregation function ")+w, is);
                    }
                } else {
                    // not an aggregation function: add to selection list

                    if (w.empty())
                        set_error("Expected argument for SELECT", is);
                    else {
                        spec.attribute_selection.selection = QuerySpec::AttributeSelection::List;
                        spec.attribute_selection.list.push_back(w);

                        selection_attr_name = w;
                    }
                }
            }

            // parse alias and unit (... AS ... UNIT ...)

            next_keyword.clear();
            std::string tmp = util::read_word(is, ",;=<>()\n");
            std::transform(tmp.begin(), tmp.end(), std::back_inserter(next_keyword), ::tolower);

            if (!error && next_keyword == "as") {
                std::string w = util::read_word(is, ",;=<>()\n");

                if (w.empty())
                    set_error("Expected alias at SELECT ... AS ", is);
                else
                    spec.aliases[selection_attr_name] = w;

                next_keyword.clear();
                tmp = util::read_word(is, ",;=<>()\n");
                std::transform(tmp.begin(), tmp.end(), std::back_inserter(next_keyword), ::tolower);
            }

            if (!error && next_keyword == "unit") {
                std::string w = util::read_word(is, ",;=()\n");

                if (w.empty())
                    set_error("Expected unit at SELECT ... UNIT ", is);
                else
                    spec.units[selection_attr_name] = w;

                next_keyword.clear();
                tmp = util::read_word(is, ",;=<>()\n");
                std::transform(tmp.begin(), tmp.end(), std::back_inserter(next_keyword), ::tolower);
            }

            if (!error && !next_keyword.empty()) {
                c = 0;
                break;
            }

            c = util::read_char(is);
        } while (!error && is.good() && c == ',');

        if (c)
            is.unget();
        if (!next_keyword.empty())
            parse_clause_from_word(next_keyword, is);
    }

    void
    parse_sort(std::istream& is) {
        std::string next_keyword;
        char c = 0;

        do {
            c = 0;
            next_keyword.clear();

            std::string arg = util::read_word(is, ",;=<>()\n");

            if (arg.empty()) {
                set_error("Sort attribute expected", is);
                return;
            }

            std::string tmp = util::read_word(is, ",;=<>()\n");
            std::transform(tmp.begin(), tmp.end(), std::back_inserter(next_keyword), ::tolower);

            if (next_keyword == "asc" ) {
                spec.sort.selection = QuerySpec::SortSelection::List;
                spec.sort.list.push_back(QuerySpec::SortSpec(arg, QuerySpec::SortSpec::Ascending));
                next_keyword.clear();
            } else if (next_keyword == "desc") {
                spec.sort.selection = QuerySpec::SortSelection::List;
                spec.sort.list.push_back(QuerySpec::SortSpec(arg, QuerySpec::SortSpec::Descending));
                next_keyword.clear();
            } else {
                spec.sort.selection = QuerySpec::SortSelection::List;
                spec.sort.list.push_back(QuerySpec::SortSpec(arg, QuerySpec::SortSpec::Ascending));

                if (!next_keyword.empty())
                    break;
            }

            c = util::read_char(is);
        } while (!error && is.good() && c == ',');

        if (c)
            is.unget();
        if (!next_keyword.empty())
            parse_clause_from_word(next_keyword, is);
    }

    QuerySpec::Condition
    parse_filter_clause(std::istream& is) {
        std::string w = util::read_word(is, ",;=<>()\n");
        std::string wl(w);

        std::transform(w.begin(), w.end(), wl.begin(), ::tolower);

        bool negate = false;

        if (wl == "not") {
            negate = true;
            w = util::read_word(is, ",;=<>()\n");
        }

        QuerySpec::Condition cond;
        cond.op        = QuerySpec::Condition::None;
        cond.attr_name = w;

        if (w.empty()) {
            set_error("Condition term expected", is);
            return cond;
        }

        char c = util::read_char(is);

        switch (c) {
        case '=':
            w = util::read_word(is, ",;=<>()\n");

            if (w.empty())
                set_error("Argument expected for '='", is);
            else {
                cond.op    = (negate ? QuerySpec::Condition::NotEqual : QuerySpec::Condition::Equal);
                cond.value = w;
            }

            break;
        case '<':
            w = util::read_word(is, ",;=<>()\n");

            if (w.empty())
                set_error("Argument expected for '<'", is);
            else {
                cond.op    = (negate ? QuerySpec::Condition::GreaterOrEqual : QuerySpec::Condition::LessThan);
                cond.value = w;
            }

            break;
        case '>':
            w = util::read_word(is, ",;=<>()\n");

            if (w.empty())
                set_error("Argument expected for '>'", is);
            else {
                cond.op = (negate ? QuerySpec::Condition::LessOrEqual : QuerySpec::Condition::GreaterThan);
                cond.value = w;
            }

            break;
        default:
            is.unget();
            cond.op = (negate ? QuerySpec::Condition::NotExist : QuerySpec::Condition::Exist);
        }

        if (cond.op == QuerySpec::Condition::None) {
            set_error("Condition term expected", is);
        }

        return cond;
    }

    void
    parse_where(std::istream& is) {
        char c = '\0';

        do {
            QuerySpec::Condition cond = parse_filter_clause(is);

            if (!error && cond.op != QuerySpec::Condition::None) {
                spec.filter.selection = QuerySpec::FilterSelection::List;
                spec.filter.list.push_back(cond);
            }

            c = util::read_char(is);
        } while (!error && is.good() && c == ',');

        if (c)
            is.unget();
    }

    void
    parse_let(std::istream& is) {
        std::string next_keyword;
        char c = 0;

        do {
            const QuerySpec::FunctionSignature* defs = Preprocessor::preprocess_defs();
            QuerySpec::PreprocessSpec pspec;

            std::string target = util::read_word(is, ",;=<>()\n");

            c = util::read_char(is);

            if (c != '=') {
                set_error(std::string("Expected \"=\" after ") + target, is);
                return;
            }

            std::string opname = util::read_word(is, ",;=<>()\n");
            std::transform(opname.begin(), opname.end(), opname.begin(), ::tolower);

            int i = 0;

            for ( ; defs[i].name && opname != defs[i].name; ++i)
                ;

            if (!defs[i].name) {
                set_error(std::string("Unknown operator ")  + opname, is);
                return;
            }

            std::vector<std::string> args = parse_arglist(is);
            int argsize = static_cast<int>(args.size());

            if (defs[i].min_args > argsize || defs[i].max_args < argsize)
                set_error(std::string("Invalid number of arguments for operator ") + opname, is);
            else {
                auto it = std::find_if(spec.preprocess_ops.begin(), spec.preprocess_ops.end(),
                                [&target](QuerySpec::PreprocessSpec& s){
                                    return s.target == target;
                            });

                if (it == spec.preprocess_ops.end()) {
                    pspec.target = target;
                    pspec.op = QuerySpec::AggregationOp(defs[i], args);
                } else {
                    set_error(target + " defined twice", is);
                    return;
                }
            }

            // parse condition (... IF ... )

            next_keyword.clear();
            std::string tmp = util::read_word(is, ",;=<>()\n");
            std::transform(tmp.begin(), tmp.end(), std::back_inserter(next_keyword), ::tolower);

            if (next_keyword == "if") {
                pspec.cond = parse_filter_clause(is);
                next_keyword.clear();
            }

            if (!error) {
                spec.preprocess_ops.emplace_back(std::move(pspec));

                if (!next_keyword.empty()) {
                    c = 0;
                    break;
                }
            }

            c = util::read_char(is);
        } while (!error && is.good() && c == ',');

        if (c)
            is.unget();
        if (!next_keyword.empty())
            parse_clause_from_word(next_keyword, is);
    }

    void
    parse_clause(Clause clause, std::istream& is) {
        switch (clause) {
        case Aggregate:
            parse_aggregate(is);
            break;
        case Format:
            parse_format(is);
            break;
        case Group:
            // we expect that "by" has already been read
            parse_groupby(is);
            break;
        case Select:
            parse_select(is);
            break;
        case Sort:
            // we expect that "by" has already been read
            parse_sort(is);
            break;
        case Where:
            parse_where(is);
            break;
        case Let:
            parse_let(is);
            break;
        case None:
            // do nothing
            break;
        }
    }

    void
    parse_clause_from_word(std::string w, std::istream& is) {
        Clause clause = get_clause_from_word(w);

        if (clause == None) {
            set_error(std::string("Expected clause keyword, got ")+w, is);
        } else {
            // special handling for "group" and "sort"
            if (clause == Group || clause == Sort) {
                std::string w2 = util::read_word(is, ",;=<>()\n");
                std::transform(w2.begin(), w2.end(), w2.begin(), ::tolower);

                if (w2 != "by") {
                    set_error(std::string("Expected clause keyword. Did you mean \"GROUP BY\"?"), is);
                    return;
                }
            }

            parse_clause(clause, is);
        }
    }

    void
    parse(std::istream& is) {
        while (!error && is.good()) {
            std::string w = util::read_word(is, ",;=<>()\n");

            if (w.empty())
                break;

            parse_clause_from_word(w, is);
        }

        if (!error && is.good()) {
            // check if there are unexpected characters left in the stream
            char c = util::read_char(is);

            if (is.good() && c != '\0' && c != '\n')
                set_error(std::string("Expected clause, got ")+c, is);
        }
    }

    CalQLParserImpl()
        : error(false), error_pos(std::istream::pos_type(-1))
    {
        spec.aggregation_ops.selection     = QuerySpec::AggregationSelection::None;
        spec.aggregation_key.selection     = QuerySpec::AttributeSelection::Default;
        spec.attribute_selection.selection = QuerySpec::AttributeSelection::Default;
        spec.filter.selection              = QuerySpec::FilterSelection::None;
        spec.sort.selection                = QuerySpec::SortSelection::None;
        spec.format.opt                    = QuerySpec::FormatSpec::Default;
    }
};

CalQLParser::CalQLParser(std::istream& is)
    : mP(new CalQLParserImpl)
{
    mP->parse(is);
}

CalQLParser::CalQLParser(const char* str)
    : mP(new CalQLParserImpl)
{
    std::istringstream is(str);
    mP->parse(is);
}

CalQLParser::~CalQLParser()
{
    mP.reset();
}

bool
CalQLParser::error() const
{
    return mP->error;
}

std::string
CalQLParser::error_msg() const
{
    return mP->error_msg;
}

std::istream::pos_type
CalQLParser::error_pos()
{
    return mP->error_pos;
}

QuerySpec
CalQLParser::spec() const
{
    return mP->spec;
}

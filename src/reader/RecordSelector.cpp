// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// RecordSelector implementation

#include "caliper/reader/QuerySpec.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"

#include "caliper/common/util/split.hpp"

#include <iostream>
#include <iterator>
#include <mutex>

using namespace cali;

namespace
{

QuerySpec::Condition
parse_clause(const std::string& str)
{
    QuerySpec::Condition clause;

    // parse "[-]attribute[(<>=)value]" string

    if (str.empty())
        return clause;

    std::string::size_type spos = 0;
    bool negate = false;

    if (str[spos] == '-') {
        ++spos;
        negate = true;
    }

    std::string::size_type opos = str.find_first_of("<>=", spos);

    clause.attr_name.assign(str, spos, opos < std::string::npos ? opos-spos : opos);

    if (opos < str.size()-1) {
        clause.value.assign(str, opos+1, std::string::npos);

        struct ops_t { char c; QuerySpec::Condition::Op op; } const ops[] = {
            // { '<', Op::Less     }, { '>', Op::Greater  },
            { '=', QuerySpec::Condition::Op::Equal },
            { 0,   QuerySpec::Condition::Op::Exist }
        };

        int i;
        for (i = 0; ops[i].c && ops[i].c != str[opos]; ++i)
            ;

        if (negate) {
            switch (ops[i].op) {
            case QuerySpec::Condition::Op::Exist:
                clause.op = QuerySpec::Condition::Op::NotExist;
                break;
            case QuerySpec::Condition::Op::Equal:
                clause.op = QuerySpec::Condition::Op::NotEqual;
                break;
            default:
                // TODO: Handle remaining conditions
                clause.op = QuerySpec::Condition::Op::None;
            }
        } else {
            clause.op = ops[i].op;
        }
    } else {
        clause.op = (negate ? QuerySpec::Condition::Op::NotExist : QuerySpec::Condition::Op::Exist);
    }

    if (clause.attr_name.empty() || (opos < std::string::npos && clause.value.empty()))
        clause.op = QuerySpec::Condition::Op::None;

    return clause;
}

}

using namespace cali;

struct RecordSelector::RecordSelectorImpl
{
    std::vector<QuerySpec::Condition> m_filters;

    struct Clause {
        QuerySpec::Condition::Op op;
        Attribute attr;
        Variant   value;
    };

    void configure(const QuerySpec& spec) {
        m_filters.clear();

        if (spec.filter.selection == QuerySpec::FilterSelection::List)
            m_filters = spec.filter.list;
    }

    void configure(const QuerySpec::Condition& cond) {
        m_filters.clear();

        if (cond.op != QuerySpec::Condition::None)
            m_filters.push_back(cond);
    }

    Clause make_clause(const CaliperMetadataAccessInterface& db, const QuerySpec::Condition& f) {
        Clause clause { f.op, Attribute::invalid, Variant() };

        clause.attr = db.get_attribute(f.attr_name);

        if (clause.attr != Attribute::invalid)
            clause.value = Variant::from_string(clause.attr.type(), f.value.c_str(), nullptr);

        return clause;
    }

    template<class Op>
    bool have_match(const Entry& entry, Op match) {
        const Node* node = entry.node();

        if (node) {
            for ( ; node && node->id() != CALI_INV_ID; node = node->parent())
                if (match(node->attribute(), node->data()))
                    return true;
        } else if  (match(entry.attribute(), entry.value()))
            return true;

        return false;
    }

    bool pass(const CaliperMetadataAccessInterface& db, const EntryList& list) {
        for (auto &f : m_filters) {
            Clause clause = make_clause(db, f);

            switch (clause.op) {
            case QuerySpec::Condition::Op::Exist:
                {
                    bool m = false;

                    for (const Entry& e : list)
                        if (have_match(e, [&clause](cali_id_t attr_id, const Variant&){
                                    return attr_id == clause.attr.id();
                                }))
                            m = true;

                    if (!m)
                        return false;
                }
                break;
            case QuerySpec::Condition::Op::NotExist:
                for (const Entry& e : list)
                    if (have_match(e, [&clause](cali_id_t attr_id, const Variant&){
                                return attr_id == clause.attr.id();
                            }))
                        return false;
                break;
            case QuerySpec::Condition::Op::Equal:
                {
                    bool m = false;

                    for (const Entry& e : list)
                        if (have_match(e, [&clause](cali_id_t attr_id, const Variant& val){
                                    return attr_id == clause.attr.id() && val == clause.value;
                                }))
                            m = true;

                    if (!m)
                        return false;
                }
                break;
            case QuerySpec::Condition::Op::NotEqual:
                for (const Entry& e : list)
                    if (have_match(e, [&clause](cali_id_t attr_id, const Variant& val){
                                return attr_id == clause.attr.id() && val == clause.value;
                            }))
                        return false;
                break;
            case QuerySpec::Condition::Op::LessThan:
            {
                bool m = false;

                for (const Entry& e : list)
                    if (have_match(e, [&clause](cali_id_t attr_id, const Variant& val){
                                return attr_id == clause.attr.id() && val < clause.value;
                            }))
                        m = true;

                if (!m)
                    return false;
            }
            break;
            case QuerySpec::Condition::Op::GreaterThan:
            {
                bool m = false;

                for (const Entry& e : list)
                    if (have_match(e, [&clause](cali_id_t attr_id, const Variant& val){
                                return attr_id == clause.attr.id() && val > clause.value;
                            }))
                        m = true;

                if (!m)
                    return false;
            }
            break;
            case QuerySpec::Condition::Op::LessOrEqual:
            {
                bool m = false;

                for (const Entry& e : list)
                    if (have_match(e, [&clause](cali_id_t attr_id, const Variant& val){
                                return attr_id == clause.attr.id() && (val < clause.value || val == clause.value);
                            }))
                        m = true;

                if (!m)
                    return false;
            }
            break;
            case QuerySpec::Condition::Op::GreaterOrEqual:
            {
                bool m = false;

                for (const Entry& e : list)
                    if (have_match(e, [&clause](cali_id_t attr_id, const Variant& val){
                                return attr_id == clause.attr.id() && (val > clause.value || val == clause.value);
                            }))
                        m = true;

                if (!m)
                    return false;
            }
            break;
            case QuerySpec::Condition::Op::None:
                break;
            }
        }

        return true;
    }
}; // RecordSelectorImpl


RecordSelector::RecordSelector(const std::string& filter_string)
    : mP { new RecordSelectorImpl }
{
    mP->m_filters = parse(filter_string);
}

RecordSelector::RecordSelector(const QuerySpec& spec)
    : mP { new RecordSelectorImpl }
{
    mP->configure(spec);
}

RecordSelector::RecordSelector(const QuerySpec::Condition& cond)
    : mP { new RecordSelectorImpl }
{
    mP->configure(cond);
}

RecordSelector::~RecordSelector()
{
    mP.reset();
}

bool
RecordSelector::pass(const CaliperMetadataAccessInterface& db, const EntryList& list)
{
    return mP->pass(db, list);
}

void
RecordSelector::operator()(CaliperMetadataAccessInterface& db, const EntryList& list, SnapshotProcessFn push) const
{
    if (mP->pass(db, list))
        push(db, list);
}

std::vector<QuerySpec::Condition>
RecordSelector::parse(const std::string& str)
{
    std::vector<QuerySpec::Condition> clauses;
    std::vector<std::string> clause_strings;

    util::split(str, ',', std::back_inserter(clause_strings));

    for (const std::string& s : clause_strings) {
        QuerySpec::Condition clause = ::parse_clause(s);

        if (clause.op != QuerySpec::Condition::Op::None)
            clauses.push_back(clause);
        else
            std::cerr << "cali-query: malformed selector clause: \"" << s << "\"" << std::endl;
    }

    return clauses;
}

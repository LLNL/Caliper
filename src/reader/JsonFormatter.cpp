// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Print web-readable table in sparse format

#include "caliper/reader/JsonFormatter.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include "caliper/common/util/split.hpp"

#include "../common/util/format_util.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <mutex>
#include <set>
#include <sstream>
#include <iostream>

using namespace cali;
using namespace std;

struct JsonFormatter::JsonFormatterImpl
{
    set<string>  m_selected;
    set<string>  m_deselected;

    OutputStream m_os;

    std::mutex   m_os_lock;

    unsigned     m_num_recs = 0;

    enum Layout {
        Records,
        Split,
        Object
    };

    Layout       m_layout         = Records;

    bool         m_opt_split      = false;
    bool         m_opt_pretty     = false;
    bool         m_opt_quote_all  = false;
    bool         m_opt_sep_nested = false;

    std::map<std::string,std::string> m_aliases;

    JsonFormatterImpl(OutputStream &os)
        : m_os(os)
        { }

    void parse(const string& field_string) {
        vector<string> fields;

        util::split(field_string, ':', back_inserter(fields));

        for (const string& s : fields) {
            if (s.size() == 0)
                continue;

            if (s[0] == '-')
                m_deselected.insert(s.substr(1, string::npos));
            else
                m_selected.insert(s);
        }
    }


    void configure(const QuerySpec& spec) {
        for (auto arg : spec.format.args) {
            if (arg == "pretty")
                m_opt_pretty = true;
            else if (arg == "quote-all")
                m_opt_quote_all = true;
            else if (arg == "separate-nested")
                m_opt_sep_nested = true;
            else if (arg == "records")
                m_layout = Records;
            else if (arg == "split")
                m_layout = Split;
            else if (arg == "object")
                m_layout = Object;
        }

        switch (spec.attribute_selection.selection) {
        case QuerySpec::AttributeSelection::Default:
        case QuerySpec::AttributeSelection::All:
            // do nothing; default is all
            break;
        case QuerySpec::AttributeSelection::None:
            // doesn't make much sense
            break;
        case QuerySpec::AttributeSelection::List:
            m_selected =
                std::set<std::string>(spec.attribute_selection.list.begin(),
                                      spec.attribute_selection.list.end());
            break;
        }

        m_aliases = spec.aliases;
    }

    inline std::string get_key(const Attribute& attr) {
        std::string name = attr.name();

        bool selected =
            m_selected.count(name) > 0 && !(m_deselected.count(name) > 0);

        if (!selected && (!m_selected.empty() || attr.is_hidden() || attr.is_global()))
            return "";

        if (attr.is_nested() && !m_opt_sep_nested)
            return "path";

        auto it = m_aliases.find(name);

        return (it == m_aliases.end() ? name : it->second);
    }

    void begin_records_section(std::ostream& os) {
        if (m_layout == Records)
            os << "[\n";
        else if (m_layout == Object)
            os << "{\n\"records\": [\n";
    }

    void end_records_section(std::ostream& os) {
        if (m_layout == Records || m_layout == Object)
            os << "\n]";
    }

    void print(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::map<std::string, std::string> noquote_kvs;
        std::map<std::string, std::string> quote_kvs;

        //
        // collect json key-value pairs for this record
        //

        for (const Entry& e : list) {
            if (e.is_reference()) {
                for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent()) {
                    std::string key = get_key(db.get_attribute(node->attribute()));

                    if (key.empty())
                        continue;

                    auto it = quote_kvs.find(key);

                    if (it == quote_kvs.end())
                        quote_kvs.emplace(std::make_pair(std::move(key), node->data().to_string()));
                    else
                        it->second = node->data().to_string().append("/").append(it->second);
                }
            } else if (e.is_immediate()) {
                Attribute attr = db.get_attribute(e.attribute());
                std::string key = get_key(attr);

                if (key.empty())
                    continue;

                cali_attr_type type = attr.type();

                if (m_opt_quote_all || type == CALI_TYPE_STRING || type == CALI_TYPE_USR)
                    quote_kvs[std::move(key)] = e.value().to_string();
                else
                    noquote_kvs[std::move(key)] = e.value().to_string();
            }
        } // for (Entry& ... )

        //
        // write the key-value pairs
        //

        std::lock_guard<std::mutex>
            g(m_os_lock);

        std::ostream* real_os = m_os.stream();

        if (noquote_kvs.size() + quote_kvs.size() > 0) {
            if (m_num_recs == 0)
                begin_records_section(*real_os);

            *real_os << (m_num_recs > 0 ? ",\n" : "") << "{";

            int count = 0;
            for (auto &p : quote_kvs) {
                *real_os << (count++ > 0 ? "," : "")
                         << (m_opt_pretty ? "\n\t" : "");
                util::write_esc_string(*real_os << "\"", p.first) << "\":";
                util::write_esc_string(*real_os << "\"", p.second) << "\"";
            }
            for (auto &p : noquote_kvs) {
                *real_os << (count++ > 0 ? "," : "")
                         << (m_opt_pretty ? "\n\t" : "");
                util::write_esc_string(*real_os << "\"", p.first) << "\":";
                util::write_esc_string(*real_os, p.second);
            }

            *real_os << (m_opt_pretty ? "\n" : "") << "}";

            ++m_num_recs;
        }
    }

    std::ostream& write_attributes(CaliperMetadataAccessInterface& db, std::ostream& os) {
        std::vector<Attribute> attrs;

        if (m_selected.empty())
            attrs = db.get_all_attributes();
        else
            for (const std::string& str : m_selected)
                attrs.push_back(db.get_attribute(str));

        // remove nested and hidden attributes
        if (!m_opt_sep_nested)
            attrs.erase(std::remove_if(attrs.begin(), attrs.end(),
                                       [](const Attribute& a){ return a.is_nested(); }),
                        attrs.end());

        attrs.erase(std::remove_if(attrs.begin(), attrs.end(),
                                   [](const Attribute& a){ return a.is_hidden(); }),
                    attrs.end());

        // write "attr1": { "prop": "value", ... } , "attr2" : ...

        int count = 0;
        for (const Attribute& a : attrs) {
            os << (count++ > 0 ? ",\n" : "\n") << (m_opt_pretty ? "\t" : "")
               << '\"' << a.name() << "\": {";

            // encode some properties
            os << "\"is_global\": "  << (a.is_global() ? "true" : "false")
               << ",\"is_nested\": " << (a.is_nested() ? "true" : "false");

            // print meta-info
            for (const Node* node = a.node(); node && node->attribute() != CALI_INV_ID; node = node->parent()) {
                util::write_esc_string(os << ",\"", db.get_attribute(node->attribute()).name()) << "\": ";
                util::write_esc_string(os << "\"", node->data().to_string()) << '\"';
            }

            os << "}";
        }

        return os;
    }

    std::ostream& write_globals(CaliperMetadataAccessInterface& db, std::ostream& os) {
        std::vector<Entry>               globals = db.get_globals();
        std::map<cali_id_t, std::string> global_vals;

        for (const Entry& e : globals)
            if (e.is_reference())
                for (const Node* node = e.node(); node && node->id() != CALI_INV_ID; node = node->parent()) {
                    std::string s = node->data().to_string();

                    if (global_vals[node->attribute()].size() > 0)
                        s.append("/").append(global_vals[node->attribute()]);

                    global_vals[node->attribute()] = s;
                }
            else
                global_vals[e.attribute()] = e.value().to_string();

        int count = 0;
        for (auto &p : global_vals) {
            if (count++ > 0)
                os << ",\n";
            if (m_opt_pretty)
                os << '\t';

            util::write_esc_string(os << '\"', db.get_attribute(p.first).name()) << "\": ";
            util::write_esc_string(os << '\"', p.second) << '\"';
        }

        return os;
    }

    void flush(CaliperMetadataAccessInterface& db) {
        std::ostream* real_os = m_os.stream();

        // close records section
        if (m_num_recs == 0)
            begin_records_section(*real_os);

        end_records_section(*real_os);

        if (m_layout == Object) {
            write_globals(db, *real_os << ",\n\"globals\": {\n") << "\n}";
            write_attributes(db, *real_os << ",\n\"attributes\": {") << "\n}";
            *real_os << "\n}"; // close object opened in begin_records_section()
        }

        *real_os << std::endl;
    }
};

JsonFormatter::JsonFormatter(OutputStream &os, const QuerySpec& spec)
    : mP { new JsonFormatterImpl(os) }
{
    mP->configure(spec);
}

JsonFormatter::~JsonFormatter()
{
    mP.reset();
}

void
JsonFormatter::process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->print(db, list);
}

void JsonFormatter::flush(CaliperMetadataAccessInterface& db, std::ostream&)
{
    mP->flush(db);
}

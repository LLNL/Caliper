// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Print web-readable table in sparse format

#include "caliper/reader/JsonSplitFormatter.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include "caliper/common/util/lockfree-tree.hpp"

#include "../common/util/format_util.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <mutex>
#include <set>
#include <sstream>
#include <iostream>



using namespace cali;

namespace
{

class Hierarchy
{
    class HierarchyNode : public IdType, public util::LockfreeIntrusiveTree<HierarchyNode>
    {
        util::LockfreeIntrusiveTree<HierarchyNode>::Node m_treenode;
        std::string m_label;
        std::string m_column;

    public:

        HierarchyNode(cali_id_t id, const std::string& label, const std::string& column)
            : IdType(id),
              util::LockfreeIntrusiveTree<HierarchyNode>(this, &HierarchyNode::m_treenode),
            m_label(label),
            m_column(column)
        { }

        const std::string& label() const { return m_label; }

        std::ostream& write_json(std::ostream& os) const {
            util::write_esc_string(os << "{ \"label\": \"",  m_label ) << "\"";
            util::write_esc_string(os << ", \"column\": \"", m_column) << "\"";

            if (parent() && parent()->id() != CALI_INV_ID)
                os << ", \"parent\": " << parent()->id();

            os << " }";

            return os;
        }
    };

    HierarchyNode*              m_root;

    std::mutex                  m_nodes_lock;
    std::vector<HierarchyNode*> m_nodes;

    void recursive_delete(HierarchyNode* node) {
        HierarchyNode* child = node->first_child();

        while (child) {
            HierarchyNode* tmp = child->next_sibling();
            recursive_delete(child);
            child = tmp;
        }

        delete node;
    }

public:

    Hierarchy()
        : m_root(new HierarchyNode(CALI_INV_ID, "", ""))
    { }

    ~Hierarchy() {
        // delete all nodes
        recursive_delete(m_root);
        m_root = nullptr;
    }

    /// \brief Return Node ID for the given path
    cali_id_t get_id(const std::vector<Entry>& vec, const std::string& column) {
        HierarchyNode* node = m_root;

        for (const Entry& e : vec) {
            HierarchyNode* parent = node;
            std::string    label  = e.value().to_string();

            for (node = parent->first_child(); node && (label != node->label()); node = node->next_sibling())
                ;

            if (!node) {
                std::lock_guard<std::mutex>
                    g(m_nodes_lock);

                node = new HierarchyNode(m_nodes.size(), label, column);
                m_nodes.push_back(node);

                parent->append(node);
            }
        }

        return node->id();
    }

    std::ostream& write_nodes(std::ostream& os) {
        std::lock_guard<std::mutex>
            g(m_nodes_lock);

        os << "\"nodes\": [";

        int count = 0;
        for (const HierarchyNode* node : m_nodes)
            node->write_json(os << (count++ > 0 ? ", " : " "));

        os << " ]";

        return os;
    }
};

}


struct JsonSplitFormatter::JsonSplitFormatterImpl
{
    bool                     m_select_all;
    std::vector<std::string> m_attr_names;

    std::map<std::string, std::string> m_aliases;

    std::mutex               m_init_lock;
    bool                     m_initialized;

    /// \brief The output columns.
    struct Column {
        std::string            title;
        std::vector<Attribute> attributes;
        bool                   is_hierarchy;

        static Column make_column(const std::string& title, const Attribute& a) {
            Column c;
            c.title        = title;
            c.attributes.push_back(a);
            c.is_hierarchy = !(a.store_as_value());
            return c;
        }
    };

    std::vector<Column>      m_columns;

    Hierarchy                m_hierarchy;

    int                      m_row_count; // protected by m_os_lock

    OutputStream             m_os;
    std::mutex               m_os_lock;


    JsonSplitFormatterImpl(OutputStream& os)
        : m_select_all(false),
          m_initialized(false),
          m_row_count(0),
          m_os(os)
    { }

    void configure(const QuerySpec& spec) {
        m_select_all = false;
        m_attr_names.clear();

        switch (spec.attribute_selection.selection) {
        case QuerySpec::AttributeSelection::Default:
        case QuerySpec::AttributeSelection::All:
            // Explicitly use aggregation key and ops if there is a GROUPBY
            if (spec.aggregation_key.selection == QuerySpec::AttributeSelection::List) {
                m_attr_names.insert(m_attr_names.end(),
                                    spec.aggregation_key.list.begin(),
                                    spec.aggregation_key.list.end());

                for (auto op : spec.aggregation_ops.list)
                    m_attr_names.push_back(Aggregator::get_aggregation_attribute_name(op));
            } else {
                m_select_all = true;
            }
            break;
        case QuerySpec::AttributeSelection::None:
            break;
        case QuerySpec::AttributeSelection::List:
            m_attr_names.insert(m_attr_names.end(),
                                spec.attribute_selection.list.begin(),
                                spec.attribute_selection.list.end());
            break;
        }

        m_aliases = spec.aliases;
    }

    void init_columns(const CaliperMetadataAccessInterface& db) {
        m_columns.clear();

        std::vector<Attribute> attrs = db.get_all_attributes();
        auto attrs_rem = attrs.end();

        if (m_select_all) {
            // filter out hidden and global attributes
            attrs_rem =
                std::remove_if(attrs.begin(), attrs.end(), [](const Attribute& a) {
                        return (a.is_hidden() || a.is_global());
                    });
        } else {
            bool select_nested =
                std::find(m_attr_names.begin(), m_attr_names.end(),
                          "prop:nested") != m_attr_names.end();

            // only include selected attributes
            attrs_rem =
                std::remove_if(attrs.begin(), attrs.end(), [this,select_nested](const Attribute& a) {
                        bool select =
                            (select_nested && a.is_nested()) ||
                                std::find(m_attr_names.begin(), m_attr_names.end(),
                                          a.name()) != m_attr_names.end();
                        return !select;
                    });
        }

        attrs.erase(attrs_rem, attrs.end());

        // Create the "path" column for all attributes with NESTED flag
        Column path;
        path.title        = "path";
        path.is_hierarchy = true;

        for (const Attribute& a : attrs) {
            if (a.is_nested())
                path.attributes.push_back(a);
            else {
                std::string name = a.name();

                auto aliasit = m_aliases.find(name);
                if (aliasit != m_aliases.end())
                    name = aliasit->second;

                m_columns.push_back(Column::make_column(name, a));
            }
        }

        if (!path.attributes.empty())
            m_columns.push_back(path);
    }

    void write_hierarchy_entry(std::ostream& os, const EntryList& list, const std::vector<Attribute>& path_attrs, const std::string& column) {
        std::vector<Entry> path;

        for (const Entry& e : list)
            for (const Node* node = e.node(); node && node->id() != CALI_INV_ID; node = node->parent())
                for (const Attribute& a : path_attrs)
                    if (node->attribute() == a.id()) {
                        path.push_back(Entry(node));
                        break;
                    }

        std::reverse(path.begin(), path.end());
        cali_id_t id = m_hierarchy.get_id(path, column);

        if (id != CALI_INV_ID)
            os << id;
        else
            os << "null";
    }

    void write_immediate_entry(std::ostream& os, const EntryList& list, const Attribute& attr) {
        cali_attr_type type = attr.type();
        bool quote = !(type == CALI_TYPE_INT || type == CALI_TYPE_UINT || type == CALI_TYPE_DOUBLE);

        for (const Entry& e : list)
            if (e.attribute() == attr.id()) {
                if (quote)
                    util::write_esc_string(os << "\"", e.value().to_string()) << "\"";
                else
                    os << e.value().to_string();

                return;
            }

        os << "null";
    }

    void process_record(const CaliperMetadataAccessInterface& db, const EntryList& list) {
        // initialize the columns on first call
        {
            std::lock_guard<std::mutex>
                g(m_init_lock);

            if (!m_initialized)
                init_columns(db);

            m_initialized = true;
        }

        std::ostringstream os;
        os << "[ ";

        int count = 0;

        for (const Column& c : m_columns) {
            if (count++ > 0)
                os << ", ";

            if (c.is_hierarchy)
                write_hierarchy_entry(os, list, c.attributes, c.title);
            else
                write_immediate_entry(os, list, c.attributes.front());
        }

        os << " ]";

        {
            std::lock_guard<std::mutex>
                g(m_os_lock);

            std::ostream* real_os = m_os.stream();

            *real_os << (m_row_count++ > 0 ? ",\n    " : "{\n  \"data\": [\n    ") << os.str();
        }
    }

    std::ostream& write_globals(std::ostream& os, CaliperMetadataAccessInterface& db) {
        std::vector<Entry> globals = db.get_globals();
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

        for (auto &p : global_vals) {
            util::write_esc_string(os << ",\n  \"", db.get_attribute(p.first).name()) << "\": ";
            util::write_esc_string(os << '"', p.second) << '\"';
        }

        return os;
    }

    std::ostream& write_column_metadata(std::ostream& os, const Column& column, CaliperMetadataAccessInterface& db) {
        os << "\"is_value\": " << (column.is_hierarchy ? "false" : "true");

        // for single-attribute columns (i.e. not "path"), write metadata
        if (column.attributes.size() == 1) {
            const Node* node = db.node(column.attributes.front().id());

            if (node)
                node = node->parent();

            for ( ; node && node->id() != CALI_INV_ID; node = node->parent()) {
                Attribute attr = db.get_attribute(node->attribute());

                // skip bootstrap info and hidden attributes
                if (attr.id() < 12 || attr.is_hidden())
                    continue;

                util::write_esc_string(os << ", \"", attr.name_c_str())        << "\": ";
                util::write_esc_string(os << "\"",   node->data().to_string()) << "\"";
            }
        }

        return os;
    }

    void write_metadata(CaliperMetadataAccessInterface& db) {
        std::ostream* real_os = m_os.stream();

        // close "data" field, start "columns"
        *real_os << (m_row_count > 0 ? "\n  ],\n" : "{\n") << "  \"columns\": [";

        {
            int count = 0;
            for (const Column& c : m_columns)
                util::write_esc_string(*real_os << (count++ > 0 ? ", " : " ") << "\"", c.title) << "\"";
        }

        // close "columns", start "column_metadata"
        *real_os << " ],\n  \"column_metadata\": [";

        {
            int count = 0;

            for (const Column& c : m_columns)
                write_column_metadata(*real_os << (count++ > 0 ? " }, { " : " { "), c, db);

            if (count > 0)
                *real_os << " } ";
        }

        // close "column_metadata", write "nodes"
        m_hierarchy.write_nodes( *real_os << " ],\n  " );

        // write globals and finish
        write_globals(*real_os, db) << "\n}" << std::endl;
    }
};


JsonSplitFormatter::JsonSplitFormatter(OutputStream &os, const QuerySpec& spec)
    : mP { new JsonSplitFormatterImpl(os) }
{
    mP->configure(spec);
}

JsonSplitFormatter::~JsonSplitFormatter()
{
    mP.reset();
}

void
JsonSplitFormatter::process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->process_record(db, list);
}

void JsonSplitFormatter::flush(CaliperMetadataAccessInterface& db, std::ostream&)
{
    mP->write_metadata(db);
}

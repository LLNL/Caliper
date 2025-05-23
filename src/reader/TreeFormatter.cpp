// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Pretty-print tree-organized snapshots

#include "TreeFormatter.h"

#include "SnapshotTableFormatter.h"
#include "SnapshotTree.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include "../common/StringConverter.h"

#include "../common/util/format_util.h"
#include "../common/util/split.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <mutex>
#include <utility>

using namespace cali;

struct TreeFormatter::TreeFormatterImpl {
    SnapshotTree m_tree;
    QuerySpec    m_spec;

    struct ColumnInfo {
        std::string display_name;
        int         width;
    };

    struct SortInfo {
        Attribute                  attribute;
        QuerySpec::SortSpec::Order order;
    };

    std::map<Attribute, ColumnInfo> m_column_info;

    int m_path_column_width;
    int m_max_column_width;

    std::map<std::string, std::string> m_aliases;

    bool m_use_nested;
    bool m_print_globals;

    std::vector<std::string> m_path_key_names;
    std::vector<Attribute>   m_path_keys;

    std::vector<SortInfo> m_sort_info;

    std::mutex m_path_key_lock;

    int column_width(int base) const
    {
        return std::max(m_max_column_width > 0 ? std::min(base, m_max_column_width) : base, 4);
    }

    void configure(const QuerySpec& spec)
    {
        // set path keys (first argument in spec.format.args)

        {
            auto it = spec.format.kwargs.find("path-attributes");
            if (it != spec.format.kwargs.end())
                util::split(it->second, ',', std::back_inserter(m_path_key_names));
        }

        // set max column width
        {
            auto it = spec.format.kwargs.find("column-width");
            if (it != spec.format.kwargs.end()) {
                bool ok = false;

                m_max_column_width = StringConverter(it->second).to_int(&ok);

                if (!ok) {
                    Log(0).stream() << "TreeFormatter: invalid column width argument \"" << it->second << "\""
                                    << std::endl;

                    m_max_column_width = -1;
                }
            }
        }

        {
            auto it = spec.format.kwargs.find("print-globals");
            if (it != spec.format.kwargs.end())
                m_print_globals = true;
        }

        {
            for (const auto path_str : { "path", "prop:nested" }) {
                auto it = std::find(m_path_key_names.begin(), m_path_key_names.end(), path_str);

                if (it != m_path_key_names.end()) {
                    m_use_nested = true;
                    m_path_key_names.erase(it);
                }
            }

            if (!m_path_key_names.empty())
                m_use_nested = false;
        }

        m_path_keys.assign(m_path_key_names.size(), Attribute());
    }

    std::vector<Attribute> get_path_keys(const CaliperMetadataAccessInterface& db)
    {
        std::vector<Attribute> path_keys;

        {
            std::lock_guard<std::mutex> g(m_path_key_lock);

            path_keys = m_path_keys;
        }

        for (std::vector<Attribute>::size_type i = 0; i < path_keys.size(); ++i)
            if (!path_keys[i]) {
                Attribute attr = db.get_attribute(m_path_key_names[i]);

                if (attr) {
                    path_keys[i] = attr;
                    std::lock_guard<std::mutex> g(m_path_key_lock);
                    m_path_keys[i] = attr;
                }
            }

        return path_keys;
    }

    void add(const CaliperMetadataAccessInterface& db, const EntryList& list)
    {
        const SnapshotTreeNode* node = nullptr;

        if (m_use_nested) {
            node =
                m_tree.add_snapshot(db, list, [](const Attribute& attr, const Variant&) { return attr.is_nested(); });
        } else {
            auto path_keys = get_path_keys(db);

            node = m_tree.add_snapshot(db, list, [&path_keys](const Attribute& attr, const Variant&) {
                return (std::find(std::begin(path_keys), std::end(path_keys), attr) != std::end(path_keys));
            });
        }

        if (!node)
            return;

        // update column widths

        for (auto& data : node->records()) {
            for (auto& entry : data) {
                int  len = entry.second.to_string().size();
                auto it  = m_column_info.find(entry.first);

                if (it == m_column_info.end()) {
                    std::string name = entry.first.name();

                    auto ait = m_spec.aliases.find(name);
                    if (ait != m_spec.aliases.end())
                        name = ait->second;
                    else {
                        Variant v = entry.first.get(db.get_attribute("attribute.alias"));

                        if (!v.empty())
                            name = v.to_string();
                    }

                    ColumnInfo ci { name, std::max<int>(len, name.size()) };

                    m_column_info.insert(std::make_pair(entry.first, ci));
                } else
                    it->second.width = std::max(it->second.width, len);
            }
        }
    }

    void init_sort_info(const CaliperMetadataAccessInterface& db)
    {
        m_sort_info.clear();

        for (const auto& s : m_spec.sort.list) {
            Attribute attr = db.get_attribute(s.attribute);
            if (attr)
                m_sort_info.push_back({ attr, s.order });
        }
    }

    std::vector<SnapshotTreeNode*> get_sorted_child_nodes(SnapshotTreeNode* parent)
    {
        std::vector<SnapshotTreeNode*> nodes;

        if (!parent)
            return nodes;

        for (SnapshotTreeNode* node = parent->first_child(); node; node = node->next_sibling())
            nodes.push_back(node);

        for (const auto& si : m_sort_info) {
            for (SnapshotTreeNode* node : nodes)
                node->sort(si.attribute, si.order == QuerySpec::SortSpec::Order::Ascending);

            if (si.order == QuerySpec::SortSpec::Order::Ascending) {
                std::stable_sort(nodes.begin(), nodes.end(), [si](SnapshotTreeNode* lhs, SnapshotTreeNode* rhs) {
                    return lhs->min_val(si.attribute) < rhs->min_val(si.attribute);
                });
            } else if (si.order == QuerySpec::SortSpec::Order::Descending) {
                std::stable_sort(nodes.begin(), nodes.end(), [si](SnapshotTreeNode* lhs, SnapshotTreeNode* rhs) {
                    return lhs->max_val(si.attribute) > rhs->max_val(si.attribute);
                });
            }
        }

        return nodes;
    }

    void recursive_print_nodes(
        SnapshotTreeNode*             node,
        int                           level,
        const std::vector<Attribute>& attributes,
        std::ostream&                 os
    )
    {
        //
        // print this node
        //

        // print the tree node label
        {
            int width = column_width(m_path_column_width);

            std::string path_str;
            path_str.append(std::min(2 * level, width - 2), ' ');

            if (2 * level >= width)
                path_str.append("..");
            else
                path_str.append(util::clamp_string(node->label_value().to_string(), width - 2 * level));

            util::pad_right(os, path_str, width);
        }

        int lines = node->records().size() > 1 ? 1 : 0;

        // print the node's data (possibly multiple records)
        for (const auto& rec : node->records()) {
            if (lines++ > 0) {
                int width = column_width(m_path_column_width);

                std::string path_str;
                path_str.append(std::min(2 * level, width - 2), ' ');

                if (2 * level >= width)
                    path_str.append("..");
                else
                    path_str.append(" |-");

                util::pad_right(os << "\n", path_str, width);
            }

            for (const Attribute& a : attributes) {
                std::string str;

                int width = column_width(m_column_info[a].width);

                {
                    auto it = std::find_if(rec.begin(), rec.end(), [&a](const std::pair<Attribute, Variant>& p) {
                        return a == p.first;
                    });

                    if (it != rec.end())
                        str = util::clamp_string(it->second.to_string(), width);
                }

                cali_attr_type t = a.type();
                bool           align_right =
                    (t == CALI_TYPE_INT || t == CALI_TYPE_UINT || t == CALI_TYPE_DOUBLE || t == CALI_TYPE_ADDR);

                if (align_right)
                    util::pad_left(os, str, width);
                else
                    util::pad_right(os, str, width);
            }
        }

        os << std::endl;

        //
        // recursively descend
        //

        for (auto child : get_sorted_child_nodes(node))
            recursive_print_nodes(child, level + 1, attributes, os);
    }

    int recursive_max_path_label_width(const SnapshotTreeNode* node, int level)
    {
        int len = 2 * level + node->label_value().to_string().length();

        for (node = node->first_child(); node; node = node->next_sibling())
            len = std::max(len, recursive_max_path_label_width(node, level + 1));

        return len;
    }

    void flush(const CaliperMetadataAccessInterface& db, std::ostream& os)
    {
        //
        // establish order of attribute columns
        //

        init_sort_info(db);

        std::vector<Attribute> attributes;

        switch (m_spec.select.selection) {
        case QuerySpec::AttributeSelection::Default:
            // auto-attributes: skip hidden and global attributes
            for (auto& p : m_column_info) {
                if (p.first.is_hidden() || p.first.is_global())
                    continue;

                attributes.push_back(p.first);
            }
            break;
        case QuerySpec::AttributeSelection::All:
            for (auto& p : m_column_info)
                attributes.push_back(p.first);
            break;
        case QuerySpec::AttributeSelection::List:
            for (const std::string& s : m_spec.select.list) {
                Attribute attr = db.get_attribute(s);

                if (!attr)
                    std::cerr << "cali-query: TreeFormatter: Attribute \"" << s << "\" not found." << std::endl;
                else
                    attributes.push_back(attr);
            }
            break;
        case QuerySpec::AttributeSelection::None:
            // keep empty list
            break;
        }

        //
        // get the maximum path column width
        //

        m_path_column_width = 4;

        {
            const SnapshotTreeNode* node = m_tree.root();

            if (node)
                for (node = node->first_child(); node; node = node->next_sibling())
                    m_path_column_width = std::max(m_path_column_width, recursive_max_path_label_width(node, 0));
        }

        //
        // print header
        //

        {
            int width = column_width(m_path_column_width);
            util::pad_right(os, util::clamp_string("Path", width), width);
        }

        for (const Attribute& a : attributes) {
            int width = column_width(m_column_info[a].width);
            util::pad_right(os, util::clamp_string(m_column_info[a].display_name, width), width);
        }

        os << std::endl;

        //
        // print tree nodes
        //

        for (auto node : get_sorted_child_nodes(m_tree.root()))
            recursive_print_nodes(node, 0, attributes, os);
    }

    TreeFormatterImpl(const QuerySpec& spec)
        : m_spec(spec), m_path_column_width(0), m_max_column_width(48), m_use_nested(true), m_print_globals(false)
    {
        configure(spec);
    }
};

TreeFormatter::TreeFormatter(const QuerySpec& spec) : mP { new TreeFormatterImpl(spec) }
{}

TreeFormatter::~TreeFormatter()
{
    mP.reset();
}

void TreeFormatter::process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->add(db, list);
}

void TreeFormatter::flush(CaliperMetadataAccessInterface& db, std::ostream& os)
{
    if (mP->m_print_globals)
        format_record_as_table(db, db.get_globals(), os);

    mP->flush(db, os);
}

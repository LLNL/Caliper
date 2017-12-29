// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by Alfredo Gimenez, gimenez1@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Print web-readable table in sparse format

#include "caliper/reader/JsonTreeFormatter.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/ContextRecord.h"
#include "caliper/common/IdType.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include "caliper/common/util/lockfree-tree.hpp"

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

    public:

        HierarchyNode(cali_id_t id, const std::string& label)
            : IdType(id),
              util::LockfreeIntrusiveTree<HierarchyNode>(this, &HierarchyNode::m_treenode),
            m_label(label)
        { }

        const std::string& label() const { return m_label; }

        std::ostream& write_json(std::ostream& os) const {
            os << "{ \"label\": \"" << m_label << "\"";

            if (parent() && parent()->id() != CALI_INV_ID)
                os << ", \"parent\": " << parent()->id();

            os << " }";
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

    void write_recursive(std::ostream& os, HierarchyNode* node) {
        os << "{ \"id\": "      << node->id()
           << ", \"label\": \"" << node->label() << "\"";

        if (node->first_child()) {
            os << ", \"children\": [";
            int count = 0;
            for (HierarchyNode* child = node->first_child(); child; child = child->next_sibling())
                write_recursive(os << ((count++ > 0) ? ", " : " "), child);
            os << " ]";
        }

        os << " }";
    }


public:

    Hierarchy()
        : m_root(new HierarchyNode(CALI_INV_ID, ""))
    { }

    ~Hierarchy() {
        // delete all nodes
        recursive_delete(m_root);
        m_root = nullptr;
    }

    /// \brief Return Node ID for the given path
    cali_id_t get_id(const std::vector<Entry>& vec) {
        HierarchyNode* node = m_root;

        for (const Entry& e : vec) {
            HierarchyNode* parent = node;
            std::string    label  = e.value().to_string();
            
            for (node = parent->first_child(); node && (label != node->label()); node = node->next_sibling())
                ;

            if (!node) {
                std::lock_guard<std::mutex>
                    g(m_nodes_lock);
                
                node = new HierarchyNode(m_nodes.size(), label);
                m_nodes.push_back(node);
                
                parent->append(node);
            }
        }

        return node->id();
    }

    std::ostream& write_recursive(std::ostream& os) {
        os << "\"hierarchy\": [";

        int count = 0;
        for (HierarchyNode* node = m_root->first_child(); node; node = node->next_sibling())
            write_recursive(os << (count++ > 0 ? ", " : " "), node);

        return os << " ]";
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


struct JsonTreeFormatter::JsonTreeFormatterImpl
{ 
    bool                     m_select_all;
    std::vector<std::string> m_attr_names;
    
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

    
    JsonTreeFormatterImpl(OutputStream& os)
        : m_initialized(false),
          m_row_count(0),
          m_os(os)
    { }

    void configure(const QuerySpec& spec) {
        switch (spec.attribute_selection.selection) {
        case QuerySpec::AttributeSelection::Default:
        case QuerySpec::AttributeSelection::All:
            m_select_all = true;
            break;
        case QuerySpec::AttributeSelection::None:
            break;
        case QuerySpec::AttributeSelection::List:
            m_attr_names = spec.attribute_selection.list;
            break;
        }
    }

    void init_columns(const CaliperMetadataAccessInterface& db) {
        m_columns.clear();
        
        std::vector<Attribute> attrs;
            
        if (m_select_all)
            attrs = db.get_attributes();
        else {
            std::vector<Attribute> tmp_attrs = db.get_attributes();

            std::copy_if(tmp_attrs.begin(), tmp_attrs.end(),
                         std::back_inserter(attrs),
                         [this](const Attribute& a) {
                             return std::find(m_attr_names.begin(), m_attr_names.end(),
                                              a.name()) != m_attr_names.end();
                         });
        }

        // Create the "path" column for all attributes with NESTED flag
        Column path;
        path.title        = "path";
        path.is_hierarchy = true;

        for (const Attribute& a : attrs) {
            if (a.is_nested())
                path.attributes.push_back(a);
            else
                m_columns.push_back(Column::make_column(a.name(), a));
        }

        if (!path.attributes.empty())
            m_columns.push_back(path);
    }

    void write_hierarchy_entry(std::ostream& os, const EntryList& list, const std::vector<Attribute>& path_attrs) {
        std::vector<Entry> path;

        for (const Entry& e : list)
            for (const Node* node = e.node(); node && node->id() != CALI_INV_ID; node = node->parent())
                for (const Attribute& a : path_attrs)
                    if (node->attribute() == a.id()) {
                        path.push_back(Entry(node));
                        break;
                    }

        std::reverse(path.begin(), path.end());
        cali_id_t id = m_hierarchy.get_id(path);

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
                    os << "\"" << e.value().to_string() << "\"";
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
                write_hierarchy_entry(os, list, c.attributes);
            else 
                write_immediate_entry(os, list, c.attributes.front());
        }

        os << " ]";

        {
            std::lock_guard<std::mutex>
                g(m_os_lock);

            m_os.stream() << (m_row_count++ > 0 ? ",\n    " : "{\n   \"data\": [\n    ") << os.str();
        }
    }

    void write_footer() {
        // close "data" field, start "columns" 
        m_os.stream() << (m_row_count > 0 ? "\n  ],\n" : "{\n") << "  \"columns\": [";

        int count = 0;
        for (const Column& c : m_columns)
            m_os.stream() << (count++ > 0 ? ", " : " ") << "\"" << c.title << "\"";

        // close "columns", write "nodes"
        m_hierarchy.write_nodes(m_os.stream() << " ],\n  ") << "\n}" << std::endl;
    }
};


JsonTreeFormatter::JsonTreeFormatter(OutputStream &os, const QuerySpec& spec)
    : mP { new JsonTreeFormatterImpl(os) }
{
    mP->configure(spec);
}

JsonTreeFormatter::~JsonTreeFormatter()
{
    mP.reset();
}

void 
JsonTreeFormatter::process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->process_record(db, list);
}

void JsonTreeFormatter::flush(CaliperMetadataAccessInterface&, std::ostream&)
{
    mP->write_footer();
}

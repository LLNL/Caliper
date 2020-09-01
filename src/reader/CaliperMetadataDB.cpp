// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// CaliperMetadataDB class definition

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>

using namespace cali;
using namespace std;

namespace
{

inline cali_id_t
map_id(cali_id_t id, const IdMap& idmap) {
    auto it = idmap.find(id);
    return it == idmap.end() ? id : it->second;
}

} // namespace

struct CaliperMetadataDB::CaliperMetadataDBImpl
{
    Node                      m_root;         ///< (Artificial) root node
    vector<Node*>             m_nodes;        ///< Node list
    mutable mutex             m_node_lock;

    Node*                     m_type_nodes[CALI_MAXTYPE+1] = { 0 };

    map<string, Node*>        m_attributes;
    mutable mutex             m_attribute_lock;

    vector<const char*>       m_string_db;
    mutable mutex             m_string_db_lock;

    vector<Entry>             m_globals;
    mutex                     m_globals_lock;

    std::map<std::string, std::string> m_attr_aliases;
    std::map<std::string, std::string> m_attr_units;

    Attribute                 m_alias_attr;
    Attribute                 m_unit_attr;

    inline Node* node(cali_id_t id) const {
        std::lock_guard<std::mutex>
            g(m_node_lock);

        return id < m_nodes.size() ? m_nodes[id] : nullptr;
    }

    inline Attribute attribute(cali_id_t id) const {
        std::lock_guard<std::mutex>
            g(m_node_lock);

        if (id >= m_nodes.size())
            return Attribute::invalid;

        return Attribute::make_attribute(m_nodes[id]);
    }

    void setup_bootstrap_nodes() {
        // Create initial nodes

        static const struct NodeInfo {
            cali_id_t id;
            cali_id_t attr_id;
            Variant   data;
            cali_id_t parent;
        }  bootstrap_nodes[] = {
            {  0, 9,  { CALI_TYPE_USR    }, CALI_INV_ID },
            {  1, 9,  { CALI_TYPE_INT    }, CALI_INV_ID },
            {  2, 9,  { CALI_TYPE_UINT   }, CALI_INV_ID },
            {  3, 9,  { CALI_TYPE_STRING }, CALI_INV_ID },
            {  4, 9,  { CALI_TYPE_ADDR   }, CALI_INV_ID },
            {  5, 9,  { CALI_TYPE_DOUBLE }, CALI_INV_ID },
            {  6, 9,  { CALI_TYPE_BOOL   }, CALI_INV_ID },
            {  7, 9,  { CALI_TYPE_TYPE   }, CALI_INV_ID },
            {  8, 8,  { CALI_TYPE_STRING, "cali.attribute.name",  19 }, 3 },
            {  9, 8,  { CALI_TYPE_STRING, "cali.attribute.type",  19 }, 7 },
            { 10, 8,  { CALI_TYPE_STRING, "cali.attribute.prop",  19 }, 1 },
            { 11, 9,  { CALI_TYPE_PTR    }, CALI_INV_ID },
            { CALI_INV_ID, CALI_INV_ID, { }, CALI_INV_ID }
        };

        // Create nodes

        m_nodes.resize(12);

        for (const NodeInfo* info = bootstrap_nodes; info->id != CALI_INV_ID; ++info) {
            Node* node = new Node(info->id, info->attr_id, info->data);

            m_nodes[info->id] = node;

            if (info->parent != CALI_INV_ID)
                m_nodes[info->parent]->append(node);
            else
                m_root.append(node);

            if (info->attr_id == 9 /* type node */)
                m_type_nodes[info->data.to_attr_type()] = node;
            else if (info->attr_id == 8 /* attribute node*/)
                m_attributes.insert(make_pair(info->data.to_string(), node));
        }
    }

    Node* create_node(cali_id_t attr_id, const Variant& data, Node* parent) {
        // NOTE: We assume that m_node_lock is locked!

        Node* node = new Node(m_nodes.size(), attr_id, data);

        m_nodes.push_back(node);

        if (parent)
            parent->append(node);

        return node;
    }

    /// \brief Make string variant from string database
    Variant make_string_variant(const char* str, size_t len) {
        if (len > 0 && str[len-1] == '\0')
            --len;

        std::lock_guard<std::mutex>
            g(m_string_db_lock);

        auto it = std::lower_bound(m_string_db.begin(), m_string_db.end(), str,
                                   [len](const char* a, const char* b) {
                                       return strncmp(a, b, len) < 0;
                                   });

        if (it != m_string_db.end() && strncmp(str, *it, len) == 0 && strlen(*it) == len)
            return Variant(CALI_TYPE_STRING, *it, len);

        char* ptr = new char[len + 1];
        strncpy(ptr, str, len);
        ptr[len] = '\0';

        m_string_db.insert(it, ptr);

        return Variant(CALI_TYPE_STRING, ptr, len);
    }

    Variant make_variant(cali_attr_type type, const std::string& str) {
        Variant ret;

        switch (type) {
        case CALI_TYPE_INV:
            break;
        case CALI_TYPE_USR:
            ret = Variant(CALI_TYPE_USR, nullptr, 0);
            Log(0).stream() << "CaliperMetadataDB: Can't read USR data at this point" << std::endl;
            break;
        case CALI_TYPE_STRING:
            ret = make_string_variant(str.data(), str.size());
            break;
        default:
            ret = Variant::from_string(type, str.c_str());
        }

        return ret;
    }

    /// Merge node given by un-mapped node info from stream with given \a idmap into DB
    /// If \a v_data is a string, it must already be in the string database!
    const Node* merge_node(cali_id_t node_id, cali_id_t attr_id, cali_id_t prnt_id, const Variant& v_data) {
        if (attribute(attr_id) == Attribute::invalid)
            attr_id = CALI_INV_ID;

        if (node_id == CALI_INV_ID || attr_id == CALI_INV_ID || v_data.empty()) {
            Log(0).stream() << "CaliperMetadataDB::merge_node(): Invalid node info: "
                            << "id="       << node_id
                            << ", attr="   << attr_id
                            << ", parent=" << prnt_id
                            << ", value="  << v_data
                            << std::endl;
            return nullptr;
        }

        Node* parent = &m_root;

        if (prnt_id != CALI_INV_ID) {
            std::lock_guard<std::mutex>
                g(m_node_lock);

            if (prnt_id >= m_nodes.size()) {
                Log(0).stream() << "CaliperMetadataDB::merge_node(): Invalid parent node " << prnt_id << " for "
                                <<  "id="       << node_id
                                << ", attr="   << attr_id
                                << ", parent=" << prnt_id
                                << ", value="  << v_data
                                << std::endl;
                return nullptr;
            }

            parent = m_nodes[prnt_id];
        }

        Node* node     = nullptr;
        bool  new_node = false;

        {
            std::lock_guard<std::mutex>
                g(m_node_lock);

            for ( node = parent->first_child(); node && !node->equals(attr_id, v_data); node = node->next_sibling() )
                ;

            if (!node) {
                node     = create_node(attr_id, v_data, parent);
                new_node = true;
            }
        }

        if (new_node && node->attribute() == Attribute::meta_attribute_keys().name_attr_id) {
            std::lock_guard<std::mutex>
                g(m_attribute_lock);

            m_attributes.insert(make_pair(string(node->data().to_string()), node));
        }

        return node;
    }

    /// Merge node given by un-mapped node info from stream with given \a idmap into DB
    /// If \a v_data is a string, it must already be in the string database!
    const Node* merge_node(cali_id_t node_id, cali_id_t attr_id, cali_id_t prnt_id, const Variant& v_data, IdMap& idmap) {
        attr_id = ::map_id(attr_id, idmap);
        prnt_id = ::map_id(prnt_id, idmap);

        const Node* node = merge_node(node_id, attr_id, prnt_id, v_data);

        if (node_id != node->id())
            idmap.insert(make_pair(node_id, node->id()));

        return node;
    }

    EntryList merge_snapshot(size_t n_nodes, const cali_id_t node_ids[],
                             size_t n_imm,   const cali_id_t attr_ids[], const Variant values[],
                             const IdMap& idmap) const
    {
        EntryList list;

        for (size_t i = 0; i < n_nodes; ++i)
            list.push_back(Entry(node(::map_id(node_ids[i], idmap))));
        for (size_t i = 0; i < n_imm; ++i)
            list.push_back(Entry(::map_id(attr_ids[i], idmap), values[i]));

        return list;
    }

    const Node* recursive_merge_node(const Node* node, const CaliperMetadataAccessInterface& db) {
        if (!node || node->id() == CALI_INV_ID)
            return nullptr;
        if (node->id() < 12)
            return m_nodes[node->id()];

        const Node* attr_node =
            recursive_merge_node(db.node(node->attribute()), db);
        const Node* parent    =
            recursive_merge_node(node->parent(), db);

        Variant v_data = node->data();

        if (v_data.type() == CALI_TYPE_STRING)
            v_data = make_string_variant(static_cast<const char*>(v_data.data()), v_data.size());

        return merge_node(node->id(), attr_node->id(), parent ? parent->id() : CALI_INV_ID, v_data);
    }

    EntryList merge_snapshot(const CaliperMetadataAccessInterface& db,
                             const EntryList& rec)
    {
        EntryList list;
        list.reserve(rec.size());

        for (const Entry& e : rec)
            if (e.is_reference())
                list.push_back(Entry(recursive_merge_node(e.node(), db)));
            else if (e.is_immediate())
                list.push_back(Entry(recursive_merge_node(db.node(e.attribute()), db)->id(), e.value()));

        return list;
    }

    void merge_global(cali_id_t node_id, const IdMap& idmap) {
        const Node* glbl_node = node(::map_id(node_id, idmap));

        if (glbl_node) {
            std::lock_guard<std::mutex>
                g(m_globals_lock);

            Entry e = Entry(glbl_node);

            if (std::find(m_globals.begin(), m_globals.end(), e) == m_globals.end())
                m_globals.push_back(e);
        }
    }

    Attribute attribute(const std::string& name) const {
        std::lock_guard<std::mutex>
            g(m_attribute_lock);

        auto it = m_attributes.find(name);

        return it == m_attributes.end() ? Attribute::invalid :
            Attribute::make_attribute(it->second);
    }

    std::vector<Attribute> get_attributes() const {
        std::lock_guard<std::mutex>
            g(m_attribute_lock);

        std::vector<Attribute> ret;
        ret.reserve(m_attributes.size());

        for (auto it : m_attributes)
            ret.push_back(Attribute::make_attribute(it.second));

        return ret;
    }

    Node* make_tree_entry(std::size_t n, const Attribute attr[], const Variant data[], Node* parent = 0) {
        Node* node = nullptr;

        if (!parent)
            parent = &m_root;

        for (size_t i = 0; i < n; ++i) {
            if (attr[i].store_as_value())
                continue;

            Variant v_data(data[i]);

            if (v_data.type() == CALI_TYPE_STRING)
                v_data = make_string_variant(static_cast<const char*>(data[i].data()), data[i].size());

            std::lock_guard<std::mutex>
                g(m_node_lock);

            for (node = parent->first_child(); node && !node->equals(attr[i].id(), v_data); node = node->next_sibling())
                ;

            if (!node)
                node = create_node(attr[i].id(), v_data, parent);

            parent = node;
        }

        return node;
    }

    Node* make_tree_entry(std::size_t n, const Node* nodelist[], Node* parent = 0) {
        Node* node = nullptr;

        if (!parent)
            parent = &m_root;

        for (size_t i = 0; i < n; ++i) {
            std::lock_guard<std::mutex>
                g(m_node_lock);

            for (node = parent->first_child(); node && !node->equals(nodelist[i]->attribute(), nodelist[i]->data()); node = node->next_sibling())
                ;

            if (!node)
                node = create_node(nodelist[i]->attribute(), nodelist[i]->data(), parent);

            parent = node;
        }

        return node;
    }

    Attribute create_attribute(const std::string& name, cali_attr_type type, int prop,
                               int meta, const Attribute* meta_attr, const Variant* meta_data) {
        std::lock_guard<std::mutex>
            g(m_attribute_lock);

        // --- Check if attribute exists

        auto it = m_attributes.lower_bound(name);

        if (it != m_attributes.end() && it->first == name)
            return Attribute::make_attribute(it->second);

        // --- Create attribute

        Node* parent = m_type_nodes[type];

        if (meta > 0)
            parent = make_tree_entry(meta, meta_attr, meta_data, parent);

        auto unit_it = m_attr_units.find(name);
        if (unit_it != m_attr_units.end()) {
            Variant v_unit(static_cast<const char*>(unit_it->second.c_str()));
            parent = make_tree_entry(1, &m_unit_attr, &v_unit, parent);
        }
        auto alias_it = m_attr_aliases.find(name);
        if (alias_it != m_attr_aliases.end()) {
            Variant v_alias(static_cast<const char*>(alias_it->second.c_str()));
            parent = make_tree_entry(1, &m_alias_attr, &v_alias, parent);
        }

        Attribute n_attr[2] = { attribute(Attribute::meta_attribute_keys().prop_attr_id),
                                attribute(Attribute::meta_attribute_keys().name_attr_id) };
        Variant   n_data[2] = { Variant(prop),
                                make_variant(CALI_TYPE_STRING, name) };

        Node* node = make_tree_entry(2, n_attr, n_data, parent);

        m_attributes.insert(make_pair(string(name), node));

        return Attribute::make_attribute(node);
    }

    std::vector<Entry> get_globals() {
        std::lock_guard<std::mutex>
            g(m_globals_lock);

        return m_globals;
    }

    void set_global(const Attribute& attr, const Variant& value) {
        if (!attr.is_global())
            return;

        std::lock_guard<std::mutex>
            g(m_globals_lock);

        if (attr.store_as_value()) {
            auto it = std::find_if(m_globals.begin(), m_globals.end(),
                                   [attr](const Entry& e) {
                                       return e.attribute() == attr.id();
                                   } );

            if (it == m_globals.end())
                m_globals.push_back(Entry(attr, value));
            else
                *it = Entry(attr, value);
        } else {
            // check if we already have this value
            if (std::find_if(m_globals.begin(), m_globals.end(),
                             [attr,value](const Entry& e) {
                                 for (const Node* node = e.node(); node; node = node->parent())
                                     if (node->equals(attr.id(), value))
                                         return true;
                                 return false;
                             } ) != m_globals.end())
                return;

            auto it = std::find_if(m_globals.begin(), m_globals.end(),
                                   [](const Entry& e) {
                                       return e.is_reference();
                                   } );

            if (it == m_globals.end() || !attr.is_autocombineable())
                m_globals.push_back(Entry(make_tree_entry(1, &attr, &value)));
            else
                *it = Entry(make_tree_entry(1, &attr, &value, node(it->node()->id())));
        }
    }

    std::vector<Entry> import_globals(CaliperMetadataAccessInterface& db, const std::vector<Entry>& import_globals) {
        std::lock_guard<std::mutex>
            g(m_globals_lock);

        m_globals.clear();

        for (const Entry& e : import_globals)
            if (e.is_reference())
                m_globals.push_back(Entry(recursive_merge_node(e.node(), db)));
            else if (e.is_immediate())
                m_globals.push_back(Entry(recursive_merge_node(db.node(e.attribute()), db)->id(), e.value()));

        return m_globals;
    }

    CaliperMetadataDBImpl()
        : m_root { CALI_INV_ID, CALI_INV_ID, { } }
        {
            setup_bootstrap_nodes();

            m_alias_attr =
                create_attribute("attribute.alias", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS,
                                 0, nullptr, nullptr);
            m_unit_attr =
                create_attribute("attribute.unit",  CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS,
                                 0, nullptr, nullptr);
        }

    ~CaliperMetadataDBImpl() {
        for (const char* str : m_string_db)
            delete[] str;
        for (Node* n : m_nodes)
            delete n;
    }
}; // CaliperMetadataDBImpl


//
// --- Public API
//

CaliperMetadataDB::CaliperMetadataDB()
    : mP { new CaliperMetadataDBImpl }
{ }

CaliperMetadataDB::~CaliperMetadataDB()
{
    if (Log::verbosity() >= 2)
        print_statistics( Log(2).stream() );
}

const Node*
CaliperMetadataDB::merge_node(cali_id_t node_id, cali_id_t attr_id, cali_id_t prnt_id, const Variant& value, IdMap& idmap)
{
    Variant v_data = value;

    // if value is a string, find or insert it in string DB
    if (v_data.type() == CALI_TYPE_STRING)
        v_data = mP->make_string_variant(static_cast<const char*>(value.data()), value.size());

    return mP->merge_node(node_id, attr_id, prnt_id, v_data, idmap);
}

const Node*
CaliperMetadataDB::merge_node(cali_id_t node_id, cali_id_t attr_id, cali_id_t prnt_id, const std::string& data, IdMap& idmap)
{
    Attribute attr = mP->attribute(::map_id(attr_id, idmap));
    Variant   v_data;

    if (attr.is_hidden()) // skip reading data from hidden entries
        v_data = Variant(CALI_TYPE_USR, nullptr, 0);
    else
        v_data = mP->make_variant(attr.type(), data);

    return mP->merge_node(node_id, attr_id, prnt_id, v_data, idmap);
}

EntryList
CaliperMetadataDB::merge_snapshot(size_t n_nodes, const cali_id_t node_ids[],
                                  size_t n_imm,   const cali_id_t attr_ids[], const Variant values[],
                                  const IdMap& idmap) const
{
    return mP->merge_snapshot(n_nodes, node_ids, n_imm, attr_ids, values, idmap);
}

EntryList
CaliperMetadataDB::merge_snapshot(const CaliperMetadataAccessInterface& db,
                                  const std::vector<Entry>& rec)
{
    return mP->merge_snapshot(db, rec);
}

Entry
CaliperMetadataDB::merge_entry(cali_id_t node_id, const IdMap& idmap)
{
    return mP->node(::map_id(node_id, idmap));
}

Entry
CaliperMetadataDB::merge_entry(cali_id_t attr_id, const std::string& data, const IdMap& idmap)
{
    Attribute attr = mP->attribute(::map_id(attr_id, idmap));

    return Entry(attr, mP->make_variant(attr.type(), data));
}

void
CaliperMetadataDB::merge_global(cali_id_t node_id, const IdMap& idmap)
{
    return mP->merge_global(node_id, idmap);
}

void
CaliperMetadataDB::merge_global(cali_id_t attr_id, const std::string& data, const IdMap& idmap)
{
    Attribute attr = mP->attribute(::map_id(attr_id, idmap));
    mP->set_global(attr, mP->make_variant(attr.type(), data));
}

Node*
CaliperMetadataDB::node(cali_id_t id) const
{
    return mP->node(id);
}

Attribute
CaliperMetadataDB::get_attribute(cali_id_t id) const
{
    return mP->attribute(id);
}

Attribute
CaliperMetadataDB::get_attribute(const std::string& name) const
{
    return mP->attribute(name);
}

std::vector<Attribute>
CaliperMetadataDB::get_all_attributes() const
{
    return mP->get_attributes();
}

Node*
CaliperMetadataDB::make_tree_entry(size_t n, const Node* nodelist[], Node* parent)
{
    return mP->make_tree_entry(n, nodelist, parent);
}

Node*
CaliperMetadataDB::make_tree_entry(size_t n, const Attribute attr[], const Variant data[], Node* parent)
{
    return mP->make_tree_entry(n, attr, data, parent);
}

Attribute
CaliperMetadataDB::create_attribute(const std::string& name, cali_attr_type type, int prop,
                                    int meta, const Attribute* meta_attr, const Variant* meta_data)
{
    return mP->create_attribute(name, type, prop, meta, meta_attr, meta_data);
}

std::vector<Entry>
CaliperMetadataDB::get_globals()
{
    return mP->get_globals();
}

void
CaliperMetadataDB::set_global(const Attribute& attr, const Variant& value)
{
    mP->set_global(attr, value);
}

std::vector<Entry>
CaliperMetadataDB::import_globals(CaliperMetadataAccessInterface& db)
{
    return mP->import_globals(db, db.get_globals());
}

std::vector<Entry>
CaliperMetadataDB::import_globals(CaliperMetadataAccessInterface& db, const std::vector<Entry>& globals)
{
    return mP->import_globals(db, globals);
}

void
CaliperMetadataDB::add_attribute_aliases(const std::map<std::string, std::string>& aliases)
{
    for (const auto &p: aliases)
        mP->m_attr_aliases[p.first] = p.second;
}

void
CaliperMetadataDB::add_attribute_units(const std::map<std::string, std::string>& units)
{
    for (const auto &p: units)
        mP->m_attr_units[p.first] = p.second;
}

std::ostream&
CaliperMetadataDB::print_statistics(std::ostream& os)
{
    os << "CaliperMetadataDB: stored " << mP->m_nodes.size()
       << " nodes, " << mP->m_string_db.size() << " strings." << std::endl;

    return os;
}

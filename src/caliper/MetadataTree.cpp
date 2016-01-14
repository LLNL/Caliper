// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
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

/// \brief MetadataTree.cpp
/// MetadataTree implementation

#include "caliper-config.h"

#include "MetadataTree.h"

#include "MemoryPool.h"

#include "Attribute.h"
#include "Node.h"
#include "Variant.h"

#include <atomic>
#include <unordered_map>

using namespace cali;

struct MetadataTree::MetadataTreeImpl
{
    Node                   m_root;
    std::atomic<unsigned>  m_node_id;

    Node*                  m_type_nodes[CALI_MAXTYPE+1];

    MetaAttributeIDs       m_meta_attributes;

    //
    // --- Constructor
    //

    MetadataTreeImpl()
        : m_root(CALI_INV_ID, CALI_INV_ID, Variant()),
          m_node_id(0),
          m_meta_attributes(MetaAttributeIDs::invalid)
        {
            bootstrap();
        }

    ~MetadataTreeImpl() {
        // Node mempools might have been removed already ... don't do anything here
        
        // for ( auto& n : m_root )
        //     n.~Node(); // Nodes have been allocated in our own pools with placement new, just call destructor here
    }

    //
    // --- bootstrap
    //

    void
    bootstrap() {
        // Create initial nodes

        static Node bootstrap_type_nodes[] = {
            {  0, 9, { CALI_TYPE_USR    },  },
            {  1, 9, { CALI_TYPE_INT    },  },
            {  2, 9, { CALI_TYPE_UINT   },  },
            {  3, 9, { CALI_TYPE_STRING },  },
            {  4, 9, { CALI_TYPE_ADDR   },  },
            {  5, 9, { CALI_TYPE_DOUBLE },  },
            {  6, 9, { CALI_TYPE_BOOL   },  },
            {  7, 9, { CALI_TYPE_TYPE   },  },
            { CALI_INV_ID, CALI_INV_ID, { } } 
        };
        static Node bootstrap_attr_nodes[] = {
            {  8, 8,  { CALI_TYPE_STRING, "cali.attribute.name",  19 } },
            {  9, 8,  { CALI_TYPE_STRING, "cali.attribute.type",  19 } },
            { 10, 8,  { CALI_TYPE_STRING, "cali.attribute.prop",  19 } },
            { CALI_INV_ID, CALI_INV_ID, { } } 
        };

        for ( Node* nodes : { bootstrap_type_nodes, bootstrap_attr_nodes } )
            for (Node* node = nodes ; node->id() != CALI_INV_ID; ++node)
                m_node_id.store(static_cast<unsigned>(node->id() + 1));

        // Fill type map

        for (Node* node = bootstrap_type_nodes ; node->id() != CALI_INV_ID; ++node)
            m_type_nodes[node->data().to_attr_type()] = node;

        // Initialize bootstrap attributes

        const MetaAttributeIDs keys = { 8, 9, 10 };
        m_meta_attributes = keys;

        struct attr_node_t { 
            Node* node; cali_attr_type type;
        } attr_nodes[] = { 
            { &bootstrap_attr_nodes[0], CALI_TYPE_STRING },
            { &bootstrap_attr_nodes[1], CALI_TYPE_TYPE   },
            { &bootstrap_attr_nodes[2], CALI_TYPE_INT    }
        };

        for ( attr_node_t p : attr_nodes ) {
            // Append to type node
            m_type_nodes[p.type]->append(p.node);
        }
    }

    //
    // --- Modifying tree operations
    //

    /// \brief Creates \param n new nodes hierarchically under \param parent 

    Node*
    create_path(MemoryPool* pool, const Attribute& attr, size_t n, const Variant* data, Node* parent = nullptr) {
        // Calculate and allocate required memory

        const size_t align = 8;
        const size_t pad   = align - sizeof(Node)%align;
        size_t total_size  = n * (sizeof(Node) + pad);

        bool   copy        = (attr.type() == CALI_TYPE_USR || attr.type() == CALI_TYPE_STRING);

        if (copy)
            for (size_t i = 0; i < n; ++i)
                total_size += data[i].size() + (align - data[i].size()%align);

        char* ptr  = static_cast<char*>(pool->allocate(total_size));
        Node* node = nullptr;

        // Create nodes

        for (size_t i = 0; i < n; ++i) {
            const void* dptr { data[i].data() };
            size_t size      { data[i].size() }; 

            if (copy)
                dptr = memcpy(ptr+sizeof(Node)+pad, dptr, size);

            node = new(ptr) 
                Node(m_node_id.fetch_add(1), attr.id(), Variant(attr.type(), dptr, size));

            if (parent)
                parent->append(node);

            ptr   += sizeof(Node)+pad + (copy ? size+(align-size%align) : 0);
            parent = node;
        }

        return node;
    }

    /// \brief Creates \param n new nodes (with different attributes) hierarchically under \param parent

    Node*
    create_path(MemoryPool* pool, size_t n, const Attribute* attr, const Variant* data, Node* parent = nullptr) {
        // Calculate and allocate required memory

        const size_t align = 8;
        const size_t pad   = align - sizeof(Node)%align;

        size_t total_size  = 0;

        for (size_t i = 0; i < n; ++i) {
            total_size += n * (sizeof(Node) + pad);

            if (attr[i].type() == CALI_TYPE_USR || attr[i].type() == CALI_TYPE_STRING)
                total_size += data[i].size() + (align - data[i].size()%align);
        }

        char* ptr  = static_cast<char*>(pool->allocate(total_size));
        Node* node = nullptr;

        // Create nodes

        for (size_t i = 0; i < n; ++i) {
            bool   copy { attr[i].type() == CALI_TYPE_USR || attr[i].type() == CALI_TYPE_STRING };

            const void* dptr { data[i].data() };
            size_t size      { data[i].size() }; 

            if (copy)
                dptr = memcpy(ptr+sizeof(Node)+pad, dptr, size);

            node = new(ptr) 
                Node(m_node_id.fetch_add(1), attr[i].id(), Variant(attr[i].type(), dptr, size));

            if (parent)
                parent->append(node);

            ptr   += sizeof(Node)+pad + (copy ? size+(align-size%align) : 0);
            parent = node;
        }

        return node;
    }

    /// \brief Retreive the given node hierarchy under \param parent
    /// Creates new nodes if necessery

    Node*
    get_path(MemoryPool* pool, const Attribute& attr, size_t n, const Variant* data, Node* parent = nullptr) {
        Node*  node = parent ? parent : &m_root;
        size_t base = 0;

        for (size_t i = 0; i < n; ++i) {
            parent = node;

            for (node = parent->first_child(); node && !node->equals(attr.id(), data[i]); node = node->next_sibling())
                ;

            if (!node)
                break;

            ++base;
        }

        if (!node)
            node = create_path(pool, attr, n-base, data+base, parent);

        return node;
    }

    /// \brief Retreive the given node hierarchy (with different attributes) under \param parent
    /// Creates new nodes if necessery

    Node*
    get_path(MemoryPool* pool, size_t n, const Attribute* attr, const Variant* data, Node* parent = nullptr) {
        Node*  node = parent ? parent : &m_root;
        size_t base = 0;

        for (size_t i = 0; i < n; ++i) {
            parent = node;

            for (node = parent->first_child(); node && !node->equals(attr[i].id(), data[i]); node = node->next_sibling())
                ;

            if (!node)
                break;

            ++base;
        }

        if (!node)
            node = create_path(pool, n-base, attr+base, data+base, parent);

        return node;
    }

    /// \brief Get a new node under \param parent that is a copy of \param node
    /// This may create a new node entry, but does not deep-copy its data

    Node*
    get_or_copy_node(MemoryPool* pool, Node* from, Node* parent = nullptr) {
        Node* node = parent ? parent : &m_root;

        for (node = parent->first_child(); node && !node->equals(from->attribute(), from->data()); node = node->next_sibling())
            ;

        if (!node) {
            char* ptr = static_cast<char*>(pool->allocate(sizeof(Node)));

            node = new(ptr) 
                Node(m_node_id.fetch_add(1), from->attribute(), from->data());

            if (parent)
                parent->append(node);
        }

        return node;
    }

    Node*
    find_hierarchy_parent(const Attribute& attr, Node* node) {
        // parent info is fixed, no need to lock
        for (Node* tmp = node ; tmp && tmp != &m_root; tmp = tmp->parent())
            if (tmp->attribute() == attr.id())
                node = tmp;

        return node ? node->parent() : &m_root;
    }

    Node*
    find_node_with_attribute(const Attribute& attr, Node* node) const {
        while (node && node->attribute() != attr.id())
            node = node->parent();

        return node;
    }

    Node*
    copy_path_without_attribute(MemoryPool* pool, const Attribute& attr, Node* node, Node* root) {
        if (!root)
            root = &m_root;
        if (!node || node == root)
            return root;

        Node* tmp = copy_path_without_attribute(pool, attr, node->parent(), root);

        if (attr.id() != node->attribute())
            tmp = get_or_copy_node(pool, node, tmp);

        return tmp;
    }

    //
    // --- High-level modifying tree ops
    //

    Node*
    remove_first_in_path(MemoryPool* pool, Node* path, const Attribute& attr) {
        Node* parent = find_node_with_attribute(attr, path);

        if (parent)
            parent = parent->parent();

        return copy_path_without_attribute(pool, attr, path, parent);
    }

    Node*
    replace_first_in_path(MemoryPool* pool, Node* path, const Attribute& attr, const Variant& data) {
        if (path)
            path = remove_first_in_path(pool, path, attr);

        return get_path(pool, attr, 1, &data, path);
    }

    Node*
    replace_all_in_path(MemoryPool* pool, Node* path, const Attribute& attr, size_t n, const Variant data[]) {
        if (path)
            path = copy_path_without_attribute(pool, attr, path, find_hierarchy_parent(attr, path));

        return get_path(pool, attr, n, data,  path);
    }
    
    Node* 
    node(cali_id_t id) {
        Node* ret = nullptr;

        for (Node* typenode : m_type_nodes)
            for (auto &n : *typenode)
                if (n.id() == id) {
                    ret = &n;
                    break;
                }

        if (!ret) 
            for (auto &n : m_root)
                if (n.id() == id) {
                    ret = &n;
                    break;
                }

        return ret;
    }

    //
    // --- I/O
    //
};


//
// --- Public interface
//

MetadataTree::MetadataTree()
    : mP(new MetadataTreeImpl)
{ }

MetadataTree::~MetadataTree()
{
    mP.reset();
}


//
// --- Modifying tree ops
// 

Node*
MetadataTree::get_path(size_t n, const Attribute attr[], const Variant data[], Node* parent, MemoryPool* pool)
{
    return mP->get_path(pool, n, attr, data, parent);
}

Node*
MetadataTree::remove_first_in_path(Node* path, const Attribute& attr, MemoryPool* pool)
{
    return mP->remove_first_in_path(pool, path, attr);
}

Node*
MetadataTree::replace_first_in_path(Node* path, const Attribute& attr, const Variant& data, MemoryPool* pool)
{
    return mP->replace_first_in_path(pool, path, attr, data);
}

Node*
MetadataTree::replace_all_in_path(Node* path, const Attribute& attr, size_t n, const Variant data[], MemoryPool* pool)
{
    return mP->replace_all_in_path(pool, path, attr, n, data);
}

//
// --- Non-modifying tree ops
//

Node*
MetadataTree::find_node_with_attribute(const Attribute& attr, Node* path) const
{
    return mP->find_node_with_attribute(attr, path);
}

//
// --- Data access ---
//

Node*
MetadataTree::node(cali_id_t id) const
{
    return mP->node(id);
}

Node*
MetadataTree::root() const
{
    return &(mP->m_root);
}

Node*
MetadataTree::type_node(cali_attr_type type) const
{
    return mP->m_type_nodes[type];
}

const MetaAttributeIDs*
MetadataTree::meta_attribute_ids() const
{
    return &(mP->m_meta_attributes);
}

//
// --- I/O ---
//

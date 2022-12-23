// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#include "caliper/common/Attribute.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Variant.h"

#include "MemoryPool.h"

#include <atomic>
#include <memory>

namespace cali
{

namespace internal
{

class MetadataTree
{
    struct NodeBlock {
        Node*  chunk;
        size_t index;
    };

    struct GlobalData {
        static const ConfigSet::Entry s_configdata[];

        ConfigSet               config;

        Node                    root;
        std::atomic<unsigned>   next_block;
        NodeBlock*              node_blocks;

        size_t                  num_blocks;
        size_t                  nodes_per_block;

        Node*                   type_nodes[CALI_MAXTYPE+1];

        //   Shared copy of the initial thread's mempool.
        // Used to merge in the pools of deleted threads.
        MemoryPool              g_mempool;

        explicit GlobalData(MemoryPool&);
        ~GlobalData();
    };

    static std::atomic<GlobalData*> mG;

    MemoryPool  m_mempool;
    NodeBlock*  m_nodeblock;

    unsigned    m_num_nodes;
    unsigned    m_num_blocks;


    bool  have_free_nodeblock(size_t n);

    Node* create_path(const Attribute& attr, size_t n, const Variant data[], Node* parent);
    Node* create_child(const Attribute& attr, const Variant& value, Node* parent);
    Node* get_or_copy_node(const Node* from, Node* parent = nullptr);
    Node* copy_path_without_attribute(const Attribute& attr, Node* node, Node* parent);

public:

    MetadataTree();

    ~MetadataTree();

    MetadataTree(const MetadataTree&) = delete;
    MetadataTree& operator = (const MetadataTree&) = delete;

    static void release();

    // --- Modifying tree operations ---

    /// \brief Get or construct a path in the tree under parent with
    ///   the attribute \a attr and the list of values in \a data
    Node*
    get_path(const Attribute& attr, size_t n, const Variant data[], Node* parent);

    /// \brief Get or construct a path in the tree under parent with
    ///   the data of the nodes given in the nodelist in the order of that list
    Node*
    get_path(size_t n, const Node* nodelist[], Node* parent);

    /// \brief Get or construct a node with \a attr, \a val under \a parent
    Node*
    get_child(const Attribute& attr, const Variant& val, Node* parent);

    Node*
    remove_first_in_path(Node* path, const Attribute& attr);

    Node*
    replace_first_in_path(Node* path, const Attribute& attr, const Variant& data);
    Node*
    replace_all_in_path(Node* path, const Attribute& attr, size_t n, const Variant data[]);

    // --- Data access ---

    Node*
    node(cali_id_t id) const {
        GlobalData* g = mG.load();

        size_t block = id / g->nodes_per_block;
        size_t index = id % g->nodes_per_block;

        if (block >= g->num_blocks || index >= g->node_blocks[block].index)
            return nullptr;

        return g->node_blocks[block].chunk + index;
    }

    Node* root() const {
        return &(mG.load()->root);
    }

    Node* type_node(cali_attr_type type) const {
        return mG.load()->type_nodes[type];
    };

    // --- I/O ---

    std::ostream&
    print_statistics(std::ostream& os) const;
};

} // namespace internal

} // namespace cali

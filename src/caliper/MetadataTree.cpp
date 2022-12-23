// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// MetadataTree implementation

#include "caliper/caliper-config.h"

#include "MetadataTree.h"

#include "MemoryPool.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Variant.h"

#include "caliper/common/util/spinlock.hpp"

#include <cstring>
#include <utility>

using namespace cali;
using namespace cali::internal;

MetadataTree::GlobalData::GlobalData(MemoryPool& pool)
    : config(RuntimeConfig::get_default_config().init("contexttree", s_configdata)),
      root(CALI_INV_ID, CALI_INV_ID, Variant()),
      next_block(1),
      node_blocks(0),
      g_mempool(pool)
{
    num_blocks  = config.get("num_blocks").to_uint();
    nodes_per_block = std::min<uint64_t>(config.get("nodes_per_block").to_uint(), 256);

    node_blocks = new NodeBlock[num_blocks];

    Node* chunk = static_cast<Node*>(pool.allocate(nodes_per_block * sizeof(Node)));

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

    for (const NodeInfo* info = bootstrap_nodes; info->id != CALI_INV_ID; ++info) {
        Node* node = new(chunk + info->id)
            Node(info->id, info->attr_id, info->data);

        if (info->parent != CALI_INV_ID)
            chunk[info->parent].append(node);
        else
            root.append(node);

        if (info->attr_id == 9 /* type node */)
            type_nodes[info->data.to_attr_type()] = node;
    }

    node_blocks[0].chunk = chunk;
    node_blocks[0].index = 12;
}

MetadataTree::GlobalData::~GlobalData()
{
    delete[] node_blocks;
}


MetadataTree::MetadataTree()
    : m_nodeblock(nullptr),
      m_num_nodes(0),
      m_num_blocks(0)
{
    GlobalData* g = mG.load();

    if (!g) {
        GlobalData* new_g = new GlobalData(m_mempool);

        // Set mG. If mG != new_g, some other thread has set it,
        // so just delete our new object.
        if (mG.compare_exchange_strong(g, new_g)) {
            m_nodeblock = new_g->node_blocks;

            ++m_num_blocks;
            m_num_nodes = m_nodeblock->index;
        } else
            delete new_g;
    }
}

MetadataTree::~MetadataTree()
{
    GlobalData* g = mG.load();
    g->g_mempool.merge(m_mempool);
}


bool MetadataTree::have_free_nodeblock(size_t n)
{
    GlobalData* g = mG.load();

    if (!m_nodeblock || m_nodeblock->index + n >= g->nodes_per_block) {
        if (g->next_block.load() >= g->num_blocks)
            return false;

        // allocate new node block

        Node* chunk = static_cast<Node*>(m_mempool.allocate(g->nodes_per_block * sizeof(Node)));

        if (!chunk)
            return false;

        size_t block_index = g->next_block++;

        if (block_index >= g->num_blocks)
            return false;

        m_nodeblock = g->node_blocks + block_index;

        m_nodeblock->chunk = chunk;
        m_nodeblock->index = 0;

        ++m_num_blocks;
    }

    return true;
}

//
// --- Modifying tree operations
//

Node*
MetadataTree::create_path(const Attribute& attr, size_t n, const Variant* data, Node* parent = nullptr)
{
    // Get a node block with sufficient free space

    if (!have_free_nodeblock(n))
        return 0;

    // Calculate and allocate required memory

    const size_t align = 8;
    size_t data_size   = 0;

    bool   copy = (attr.type() == CALI_TYPE_STRING || attr.type() == CALI_TYPE_USR);
    char*  ptr  = nullptr;

    if (copy) {
        for (size_t i = 0; i < n; ++i)
            data_size += data[i].size() + (align - data[i].size()%align);

        ptr = static_cast<char*>(m_mempool.allocate(data_size));

        if (!ptr)
            return nullptr;
    }

    Node* node = nullptr;

    // Create nodes

    GlobalData* g = mG.load();

    for (size_t i = 0; i < n; ++i) {
        const void* dptr { data[i].data() };
        size_t size      { data[i].size() };

        if (copy) {
            dptr = memcpy(ptr, dptr, size);
            ptr += size+(align-size%align);
        }

        size_t index = m_nodeblock->index++;

        node = new(m_nodeblock->chunk + index)
            Node((m_nodeblock - g->node_blocks) * g->nodes_per_block + index, attr.id(), Variant(attr.type(), dptr, size));

        if (parent)
            parent->append(node);

        parent = node;
    }

    m_num_nodes += n;

    return node;
}

Node*
MetadataTree::create_child(const Attribute& attr, const Variant& value, Node* parent)
{
    // Get a node block with sufficient free space

    if (!have_free_nodeblock(1))
        return nullptr;

    void* ptr = nullptr;

    if (value.has_unmanaged_data())
        ptr = m_mempool.allocate(value.size());

    size_t index = m_nodeblock->index++;
    GlobalData* g = mG.load();

    Node* node = new(m_nodeblock->chunk + index)
        Node((m_nodeblock - g->node_blocks) * g->nodes_per_block + index, attr.id(), value.copy(ptr));

    if (parent)
        parent->append(node);

    ++m_num_nodes;

    return node;
}

Node*
MetadataTree::get_path(const Attribute& attr, size_t n, const Variant* data, Node* parent = nullptr)
{
    Node*  node = parent ? parent : &(mG.load()->root);
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
        node = create_path(attr, n-base, data+base, parent);

    return node;
}

Node*
MetadataTree::get_path(size_t n, const Node* nodelist[], Node* parent = nullptr) {
    Node* node = parent;

    for (size_t i = 0; i < n; ++i)
        if (nodelist[i])
            node = get_or_copy_node(nodelist[i], node);

    return node;
}

Node*
MetadataTree::get_or_copy_node(const Node* from, Node* parent)
{
    GlobalData* g = mG.load();

    if (!parent)
        parent = root();

    Node* node = nullptr;

    for ( node = parent->first_child(); node && !node->equals(from->attribute(), from->data()); node = node->next_sibling())
        ;

    if (!node) {
        if (!have_free_nodeblock(1))
            return nullptr;

        size_t index = m_nodeblock->index++;

        node = new(m_nodeblock->chunk + index)
            Node((m_nodeblock - g->node_blocks) * g->nodes_per_block + index, from->attribute(), from->data());

        parent->append(node);

        ++m_num_nodes;
    }

    return node;
}

Node*
MetadataTree::copy_path_without_attribute(const Attribute& attr, Node* node, Node* parent)
{
    if (!parent)
        parent = root();
    if (!node || node == parent)
        return parent;

    Node* tmp = copy_path_without_attribute(attr, node->parent(), parent);

    if (attr.id() != node->attribute())
        tmp = get_or_copy_node(node, tmp);

    return tmp;
}

Node*
MetadataTree::remove_first_in_path(Node* path, const Attribute& attr)
{
    Node* node = path;

    for ( ; node && node->attribute() != attr.id(); node = node->parent())
        ;

    if (node)
        node = node->parent();

    return copy_path_without_attribute(attr, path, node);
}

Node*
MetadataTree::replace_first_in_path(Node* path, const Attribute& attr, const Variant& data)
{
    if (path)
        path = remove_first_in_path(path, attr);

    return get_child(attr, data, path);
}

Node*
MetadataTree::replace_all_in_path(Node* path, const Attribute& attr, size_t n, const Variant data[])
{
    Node* parent = path;

    for (Node* tmp = path; tmp; tmp = tmp->parent())
        if (tmp->attribute() == attr.id())
            parent = tmp;

    parent = parent ? parent->parent() : root();

    return get_path(attr, n, data, copy_path_without_attribute(attr, path, parent));
}

Node*
MetadataTree::get_child(const Attribute& attr, const Variant& val, Node* parent)
{
    if (!parent)
        parent = root();

    cali_id_t attr_id = attr.id();

    for (Node* node = parent->first_child(); node; node = node->next_sibling())
        if (node->equals(attr_id, val))
            return node;

    return create_child(attr, val, parent);
}

void MetadataTree::release()
{
    GlobalData* g = mG.exchange(nullptr);
    delete g;
}

//
// --- I/O
//

std::ostream&
MetadataTree::print_statistics(std::ostream& os) const
{
    m_mempool.print_statistics(
            os << "  Metadata tree: " << m_num_blocks << " blocks, " << m_num_nodes << " nodes\n   "
        );

	return os;
}

std::atomic<MetadataTree::GlobalData*> MetadataTree::mG;

const ConfigSet::Entry MetadataTree::GlobalData::s_configdata[] = {
    // key, type, value, short description, long description
    { "nodes_per_block", CALI_TYPE_UINT, "256",
      "Number of context tree nodes in a node block",
      "Number of context tree nodes in a node block",
    },
    { "num_blocks", CALI_TYPE_UINT, "16384",
      "Maximum number of context tree node blocks",
      "Maximum number of context tree node blocks"
    },
    ConfigSet::Terminator
};

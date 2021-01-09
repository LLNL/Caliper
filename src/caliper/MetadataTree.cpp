// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
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

#include <atomic>
#include <cstring>
#include <mutex>
#include <utility>

// #define METADATATREE_BENCHMARK

using namespace cali;

struct MetadataTree::MetadataTreeImpl
{
    struct NodeBlock {
        Node*  chunk;
        size_t index;
    };

    struct GlobalData {
        GlobalData(MemoryPool& pool)
            : config(RuntimeConfig::get_default_config().init("contexttree", s_configdata)),
              root(CALI_INV_ID, CALI_INV_ID, Variant()),
              next_block(1),
              node_blocks(0),
              g_mempool(pool)
            {
                num_blocks      = config.get("num_blocks").to_uint();
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

        ~GlobalData() {
            delete[] node_blocks;
        }

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
    };

    static std::atomic<GlobalData*> mG;

    MemoryPool  m_mempool;
    NodeBlock*  m_nodeblock;

    unsigned    m_num_nodes;
    unsigned    m_num_blocks;

#ifdef METADATATREE_BENCHMARK
    unsigned    m_num_lookups;
    unsigned    m_max_lookup_ops;
    unsigned    m_tot_lookup_ops;
#endif

    //
    // --- Constructor
    //

    MetadataTreeImpl()
        : m_nodeblock(nullptr),
          m_num_nodes(0),
          m_num_blocks(0)
#ifdef METADATATREE_BENCHMARK
        , m_num_lookups(0),
          m_max_lookup_ops(0),
          m_tot_lookup_ops(0)
#endif
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

    ~MetadataTreeImpl() {
        GlobalData* g = mG.load();
        g->g_mempool.merge(m_mempool);
    }

    /// \brief Get a node block with \param n free entries

    bool have_free_nodeblock(size_t n) {
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

    /// \brief Creates \param n new nodes hierarchically under \param parent

    Node*
    create_path(const Attribute& attr, size_t n, const Variant* data, Node* parent = nullptr) {
        // Get a node block with sufficient free space

        if (!have_free_nodeblock(n))
            return 0;

        // Calculate and allocate required memory

        const size_t align = 8;
        size_t data_size   = 0;

        bool   copy        = (attr.type() == CALI_TYPE_USR || attr.type() == CALI_TYPE_STRING);
        char*  ptr         = nullptr;

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

    /// \brief Creates \param n new nodes (with different attributes) hierarchically under \param parent

    Node*
    create_path(size_t n, const Attribute* attr, const Variant* data, Node* parent = nullptr) {
        // Get a node block with sufficient free space

        if (!have_free_nodeblock(n))
            return 0;

        // Calculate and allocate required memory

        const size_t align = 8;
        size_t data_size   = 0;

        for (size_t i = 0; i < n; ++i)
            if (attr[i].type() == CALI_TYPE_USR || attr[i].type() == CALI_TYPE_STRING)
                data_size += data[i].size() + (align - data[i].size()%align);

        char* ptr  = nullptr;

        if (data_size > 0) {
            ptr = static_cast<char*>(m_mempool.allocate(data_size));

            if (!ptr)
                return nullptr;
        }

        Node* node = nullptr;
        GlobalData* g = mG.load();

        // Create nodes

        for (size_t i = 0; i < n; ++i) {
            bool   copy { attr[i].type() == CALI_TYPE_USR || attr[i].type() == CALI_TYPE_STRING };

            const void* dptr { data[i].data() };
            size_t size      { data[i].size() };

            if (copy) {
                dptr = memcpy(ptr, dptr, size);
                ptr += size+(align-size%align);
            }

            size_t index = m_nodeblock->index++;

            node = new(m_nodeblock->chunk + index)
                Node((m_nodeblock - g->node_blocks) * g->nodes_per_block + index, attr[i].id(), Variant(attr[i].type(), dptr, size));

            if (parent)
                parent->append(node);

            parent = node;
        }

        m_num_nodes += n;

        return node;
    }

    /// \brief Retreive the given node hierarchy under \param parent
    /// Creates new nodes if necessery

    Node*
    get_path(const Attribute& attr, size_t n, const Variant* data, Node* parent = nullptr) {
        Node*  node = parent ? parent : &(mG.load()->root);
        size_t base = 0;

        for (size_t i = 0; i < n; ++i) {
            parent = node;

#ifdef METADATATREE_BENCHMARK
            unsigned num_ops = 1;
#endif

            for (node = parent->first_child(); node && !node->equals(attr.id(), data[i]); node = node->next_sibling())
#ifdef METADATATREE_BENCHMARK
                ++num_ops
#endif
                ;

#ifdef METADATATREE_BENCHMARK
            ++m_num_lookups;
            m_tot_lookup_ops += num_ops;
            m_max_lookup_ops  = std::max(m_max_lookup_ops, num_ops);
#endif

            if (!node)
                break;

            ++base;
        }

        if (!node)
            node = create_path(attr, n-base, data+base, parent);

        return node;
    }

    /// \brief Retreive the given node hierarchy (with different attributes) under \param parent
    /// Creates new nodes if necessery

    Node*
    get_path(size_t n, const Attribute* attr, const Variant* data, Node* parent = nullptr) {
        Node*  node = parent ? parent : &(mG.load()->root);
        size_t base = 0;

        for (size_t i = 0; i < n; ++i) {
            parent = node;

#ifdef METADATATREE_BENCHMARK
            unsigned num_ops = 1;
#endif

            for (node = parent->first_child(); node && !node->equals(attr[i].id(), data[i]); node = node->next_sibling())
#ifdef METADATATREE_BENCHMARK
                ++num_ops
#endif
                ;

#ifdef METADATATREE_BENCHMARK
            ++m_num_lookups;
            m_tot_lookup_ops += num_ops;
            m_max_lookup_ops  = std::max(m_max_lookup_ops, num_ops);
#endif

            if (!node)
                break;

            ++base;
        }

        if (!node)
            node = create_path(n-base, attr+base, data+base, parent);

        return node;
    }

    Node*
    get_path(size_t n, const Node* nodelist[], Node* parent = nullptr) {
        Node* node = parent;

        for (size_t i = 0; i < n; ++i)
            if (nodelist[i])
                node = get_or_copy_node(nodelist[i], node);

        return node;
    }

    /// \brief Get a new node under \param parent that is a copy of \param node
    /// This may create a new node entry, but does not deep-copy its data

    Node*
    get_or_copy_node(const Node* from, Node* parent = nullptr) {
        GlobalData* g = mG.load();

        if (!parent)
            parent = &(g->root);

        Node* node = nullptr;

#ifdef METADATATREE_BENCHMARK
            unsigned num_ops = 1;
#endif

        for ( node = parent->first_child(); node && !node->equals(from->attribute(), from->data()); node = node->next_sibling())
#ifdef METADATATREE_BENCHMARK
                ++num_ops
#endif
            ;

#ifdef METADATATREE_BENCHMARK
            ++m_num_lookups;
            m_tot_lookup_ops += num_ops;
            m_max_lookup_ops  = std::max(m_max_lookup_ops, num_ops);
#endif

        if (!node) {
            if (!have_free_nodeblock(1))
                return 0;

            size_t index = m_nodeblock->index++;

            node = new(m_nodeblock->chunk + index)
                Node((m_nodeblock - g->node_blocks) * g->nodes_per_block + index, from->attribute(), from->data());

            parent->append(node);

            ++m_num_nodes;
        }

        return node;
    }

    Node*
    find_hierarchy_parent(const Attribute& attr, Node* node) {
        GlobalData* g = mG.load();

        // parent info is fixed, no need to lock
        for (Node* tmp = node ; tmp && tmp != &(g->root); tmp = tmp->parent())
            if (tmp->attribute() == attr.id())
                node = tmp;

        return node ? node->parent() : &(g->root);
    }

    Node*
    find_node_with_attribute(const Attribute& attr, Node* node) const {
        while (node && node->attribute() != attr.id())
            node = node->parent();

        return node;
    }

    Node*
    copy_path_without_attribute(const Attribute& attr, Node* node, Node* root) {
        if (!root)
            root = &(mG.load()->root);
        if (!node || node == root)
            return root;

        Node* tmp = copy_path_without_attribute(attr, node->parent(), root);

        if (attr.id() != node->attribute())
            tmp = get_or_copy_node(node, tmp);

        return tmp;
    }

    //
    // --- High-level modifying tree ops
    //

    Node*
    remove_first_in_path(Node* path, const Attribute& attr) {
        Node* parent = find_node_with_attribute(attr, path);

        if (parent)
            parent = parent->parent();

        return copy_path_without_attribute(attr, path, parent);
    }

    Node*
    replace_first_in_path(Node* path, const Attribute& attr, const Variant& data) {
        if (path)
            path = remove_first_in_path(path, attr);

        return get_path(attr, 1, &data, path);
    }

    Node*
    replace_all_in_path(Node* path, const Attribute& attr, size_t n, const Variant data[]) {
        if (path)
            path = copy_path_without_attribute(attr, path, find_hierarchy_parent(attr, path));

        return get_path(attr, n, data,  path);
    }

    Node*
    node(cali_id_t id) const {
        GlobalData* g = mG.load();

        size_t block = id / g->nodes_per_block;
        size_t index = id % g->nodes_per_block;

        if (block >= g->num_blocks || index >= g->node_blocks[block].index)
            return nullptr;

        return g->node_blocks[block].chunk + index;
    }

    //
    // --- I/O
    //

    std::ostream&
    print_statistics(std::ostream& os) const {
        m_mempool.print_statistics(
            os << "  Metadata tree: " << m_num_blocks << " blocks, " << m_num_nodes << " nodes\n    "
#ifdef METADATATREE_BENCHMARK
            << "  "
            << m_num_lookups << " lookups with "
            << m_tot_lookup_ops << " ops total (max "
            << m_max_lookup_ops << " , avg "
            << (m_num_lookups > 0 ? static_cast<double>(m_tot_lookup_ops) / m_num_lookups : 0.0)
            << ").\n      "
#endif
                                   );

	return os;
    }
};

std::atomic<MetadataTree::MetadataTreeImpl::GlobalData*> MetadataTree::MetadataTreeImpl::mG;

const ConfigSet::Entry MetadataTree::MetadataTreeImpl::GlobalData::s_configdata[] = {
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

void MetadataTree::release()
{
    MetadataTreeImpl::GlobalData* g = MetadataTreeImpl::mG.exchange(nullptr);
    delete g;
}

//
// --- Modifying tree ops
//

Node*
MetadataTree::get_path(size_t n, const Attribute attr[], const Variant data[], Node* parent)
{
    return mP->get_path(n, attr, data, parent);
}

Node*
MetadataTree::get_path(const Attribute& attr, size_t n, const Variant data[], Node* parent)
{
    return mP->get_path(attr, n, data, parent);
}

Node*
MetadataTree::get_path(size_t n, const Node* nodelist[], Node* parent)
{
    return mP->get_path(n, nodelist, parent);
}

Node*
MetadataTree::remove_first_in_path(Node* path, const Attribute& attr)
{
    return mP->remove_first_in_path(path, attr);
}

Node*
MetadataTree::replace_first_in_path(Node* path, const Attribute& attr, const Variant& data)
{
    return mP->replace_first_in_path(path, attr, data);
}

Node*
MetadataTree::replace_all_in_path(Node* path, const Attribute& attr, size_t n, const Variant data[])
{
    return mP->replace_all_in_path(path, attr, n, data);
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
    return &(MetadataTreeImpl::mG.load()->root);
}

Node*
MetadataTree::type_node(cali_attr_type type) const
{
    return MetadataTreeImpl::mG.load()->type_nodes[type];
}

//
// --- I/O ---
//

std::ostream&
MetadataTree::print_statistics(std::ostream& os) const
{
    return mP->print_statistics(os);
}

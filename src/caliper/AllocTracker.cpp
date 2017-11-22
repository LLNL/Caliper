#include "caliper/Caliper.h"
#include "caliper/AllocTracker.h"
#include "caliper/SnapshotRecord.h"
#include "caliper/common/Attribute.h"

#include "caliper/MemoryPool.h"

#include <cstring>
#include <mutex>
#include <iostream>

using namespace cali;
using namespace DataTracker;

namespace cali
{
extern Attribute alloc_label_attr;
extern Attribute alloc_fn_attr;
}

extern cali_id_t cali_alloc_fn_attr_id;
extern cali_id_t cali_alloc_label_attr_id;
extern cali_id_t cali_alloc_addr_attr_id;
extern cali_id_t cali_alloc_elem_size_attr_id;
extern cali_id_t cali_alloc_num_elems_attr_id;
extern cali_id_t cali_alloc_total_size_attr_id;

const Allocation Allocation::invalid("INVALID", 0, 0, nullptr, 0);
const std::string AllocTracker::cali_alloc("Caliper Alloc");
const std::string AllocTracker::cali_free("Caliper Free");

size_t Allocation::num_bytes(const size_t elem_size,
                             const size_t dimensions[],
                             const size_t num_dimensions) {
    return elem_size*num_elems(dimensions, num_dimensions);
}

size_t Allocation::num_elems(const size_t dimensions[],
                             const size_t num_dimensions) {
    size_t ret = 1;
    for(int i=0; i<num_dimensions; i++) 
        ret *= dimensions[i];
    return ret;
}

Allocation::Allocation(const std::string &label,
                       const uint64_t start_address,
                       const size_t elem_size,
                       const size_t dimensions[],
                       const size_t num_dimensions)
        : m_label(label),
          m_start_address(start_address),
          m_elem_size(elem_size),
          m_dimensions(new size_t[num_dimensions]),
          m_num_elems(num_elems(dimensions, num_dimensions)),
          m_bytes(num_bytes(elem_size, dimensions, num_dimensions)),
          m_end_address(start_address + m_bytes),
          m_num_dimensions(num_dimensions),
          m_index_ret(new size_t[num_dimensions])
{ 
    for(int i=0; i<num_dimensions; i++) 
        m_dimensions[i] = dimensions[i];
}

Allocation::Allocation(const Allocation &other)
        : m_label(other.m_label),
          m_start_address(other.m_start_address),
          m_elem_size(other.m_elem_size),
          m_dimensions(other.m_dimensions),
          m_num_elems(other.m_num_elems),
          m_bytes(other.m_bytes),
          m_end_address(other.m_end_address),
          m_num_dimensions(other.m_num_dimensions),
          m_index_ret(new size_t[m_num_dimensions])
{
    std::memcpy(m_index_ret, other.m_index_ret, m_num_dimensions*sizeof(size_t));
}

Allocation::~Allocation() {
    delete[] m_index_ret;
}

bool Allocation::contains(uint64_t address) {
    return m_start_address <= address && m_end_address > address;
}

size_t Allocation::index_1D(uint64_t address) {
    return (address - m_start_address) / m_elem_size;
}

const size_t * Allocation::index_ND(uint64_t address) {

    size_t offset = index_1D(address);

    size_t d;
    for (d = 0; d<m_num_dimensions-1; d++) {
        m_index_ret[d] = offset % m_dimensions[d];
        offset = offset / m_dimensions[d];
    }
    m_index_ret[d] = offset;

    return m_index_ret;
}

struct AllocTracker::AllocTree {

    std::mutex process_mutex;
    static thread_local std::mutex thread_mutex;

    enum HAND {
        LEFT = -1,
        NA = 0,
        RIGHT = 1
    };

    struct AllocNode {

        Allocation *allocation;
        AllocNode *parent;
        AllocNode *left;
        AllocNode *right;
        HAND handedness;
        uint64_t key;

        AllocNode(Allocation *allocation,
                  AllocNode *parent,
                  AllocNode *left,
                  AllocNode *right,
                  HAND handedness)
                : allocation(allocation),
                  parent(parent),
                  left(left),
                  right(right),
                  handedness(handedness),
                  key(allocation->m_start_address)
        { }

        ~AllocNode() {
            delete allocation;
        }

        AllocNode* insert(Allocation *allocation) {
            if (allocation->m_start_address < key) {
                if (left)
                    return left->insert(allocation);
                else {
                    AllocNode *newNode = new AllocNode(allocation, this, nullptr, nullptr, LEFT);
                    left = newNode;
                    return newNode;
                }
            }
            else if (allocation->m_start_address > key) {
                if (right)
                    return right->insert(allocation);
                else {
                    AllocNode *newNode = new AllocNode(allocation, this, nullptr, nullptr, RIGHT);
                    right = newNode;
                    return newNode;
                }
            }
            else {
                // Duplicate allocation, replace this one
                delete this->allocation;
                this->allocation = allocation;
                return this;
            }
        }

        AllocNode* find_allocation_containing(uint64_t address) {
            if (address < key) {
                if (left)
                    return left->find_allocation_containing(address);
                else 
                    return nullptr;
            }
            else { // if (allocation.start_address >= key) {
                if (allocation->contains(address))
                    return this;
                else if (right)
                    return right->find_allocation_containing(address);
                else 
                    return nullptr;
            }
        }

        AllocNode* findMin() {
            if (left)
                return left->findMin();
            return this;
        }

        AllocNode* findMax() {
            if (right)
                return right->findMax();
            return this;
        }
    }; // AllocNode

    AllocNode *root;
    std::map<uint64_t, AllocNode*> alloc_map;
    bool m_map_only;

    void set_map_only(bool map_only) {
        m_map_only = map_only;
    }


    AllocTree(AllocNode *root = nullptr, bool map_only = false) 
        : root(root),
          m_map_only(map_only)
    { }

    void rotate_left(AllocNode *node) {
        if (node->left) {
            node->left->parent = node->parent;
            node->left->handedness = RIGHT;
        }

        AllocNode *npp = node->parent->parent;
        HAND np_hand = node->parent->handedness;

        node->parent->parent = node;
        node->parent->handedness = LEFT;
        node->parent->right = node->left;

        node->left = node->parent;
        node->parent = npp;
        node->handedness = (npp ? np_hand : NA);
    }

    void rotate_right(AllocNode *node) {
        if (node->right) {
            node->right->parent = node->parent;
            node->right->handedness = LEFT;
        }

        AllocNode *npp = node->parent->parent;
        HAND np_hand = node->parent->handedness;

        node->parent->parent = node;
        node->parent->handedness = RIGHT;
        node->parent->left = node->right;

        node->right = node->parent;
        node->parent = npp;
        node->handedness = (npp ? np_hand : NA);
    }

    void splay(AllocNode *node) {
        while (node->parent) {
            if (node->parent->parent == nullptr) {
                // Case: single zig
                if (node->handedness == RIGHT) {
                    // Case zig left
                    rotate_left(node);
                } else if (node->handedness == LEFT) {
                    // Case zig right
                    rotate_right(node);
                }
            } else if (node->handedness == RIGHT && node->parent->handedness == RIGHT) {
                // Case zig zig left
                rotate_left(node);
                rotate_left(node);
            } else if (node->handedness == LEFT && node->parent->handedness == LEFT) {
                // Case zig zig right
                rotate_right(node);
                rotate_right(node);
            } else if (node->handedness == RIGHT && node->parent->handedness == LEFT) {
                // Case zig zag left
                rotate_left(node);
                rotate_right(node);
            } else if (node->handedness == LEFT && node->parent->handedness == RIGHT) {
                // Case zig zag right
                rotate_right(node);
                rotate_left(node);
            }
        }

        // Splayed, root is now node
        root = node;
    }

    AllocNode* insert(Allocation *allocation) {

        std::unique_lock<std::mutex> thread_lock(thread_mutex, std::try_to_lock);
        if(!thread_lock.owns_lock()){
            return nullptr;
        }

        std::unique_lock<std::mutex> process_lock(process_mutex);

        if (m_map_only) {
            AllocNode *newNode = 
                new AllocNode(allocation, nullptr, nullptr, nullptr, NA);
            alloc_map[allocation->m_start_address] = newNode;
            return newNode;
        }

        if (root == nullptr) {
            root = new AllocNode(allocation, nullptr, nullptr, nullptr, NA);
        }
        else {
            AllocNode *newNode = root->insert(allocation);
            splay(newNode);
        }
        alloc_map[root->key] = root;

        return root;
    }

    Allocation remove(uint64_t start_address) {

        std::unique_lock<std::mutex> thread_lock(thread_mutex, std::try_to_lock);
        if(!thread_lock.owns_lock()){
            return Allocation::invalid;
        }

        std::unique_lock<std::mutex> process_lock(process_mutex);

        if (m_map_only) {
            AllocNode *node = alloc_map[start_address];
            if (node) {
                Allocation ret(*node->allocation);
                delete node;
                alloc_map.erase(start_address);
                return ret;
            }
        }

        if (root == nullptr) {
            // Nothing to do here
        }
        else {
            AllocNode *node = alloc_map[start_address];
            if (node) {
                splay(node);
                AllocTree leftTree(node->left);
                if (leftTree.root) {
                    leftTree.root->parent = nullptr;
                    leftTree.root->handedness = NA;
                    AllocNode *lMax = leftTree.root->findMax();
                    leftTree.splay(lMax);
                    root = lMax;
                    root->right = node->right;
                    if (root->right)
                        root->right->parent = root;
                } else if (node->right) {
                    root = node->right;
                    root->parent = nullptr;
                    root->handedness = NA;
                } else {
                    root = nullptr;
                }
                Allocation ret = *(node->allocation);
                delete node;
                alloc_map.erase(start_address);
                return ret;
            }
        }
        return Allocation::invalid;
    }

    AllocNode* get_allocation_at(uint64_t address) {
        if (alloc_map.find(address) == alloc_map.end()) {
            return nullptr;
        } else {
            return alloc_map[address];
        }
    }

    AllocNode* find_allocation_containing(uint64_t address) {

        std::unique_lock<std::mutex> thread_lock(thread_mutex, std::try_to_lock);
        if(!thread_lock.owns_lock()){
            return nullptr;
        }

        std::unique_lock<std::mutex> process_lock(process_mutex);

        if (root == nullptr) {
            // Nothing to find here
            return nullptr;
        }

        AllocNode *node = root->find_allocation_containing(address);
        if (node) {
            splay(node);
            return node;
        }

        return nullptr;
    }
};

thread_local std::mutex AllocTracker::AllocTree::thread_mutex;

AllocTracker::AllocTracker(bool track_ranges) 
    : alloc_tree(new AllocTree(nullptr, !track_ranges))
{ }

void 
AllocTracker::set_track_ranges(bool track_ranges) {
    alloc_tree->set_map_only(!track_ranges);
}

void 
AllocTracker::add_allocation(const std::string &label,
                             const uint64_t addr,
                             const size_t elem_size,
                             const size_t dimensions[],
                             const size_t num_dimensions,
                             const std::string fn_name,
                             bool record_snapshot) {
    // Insert into splay tree
    Allocation *a = new Allocation(label, addr, elem_size, dimensions, num_dimensions);
    AllocTree::AllocNode *newNode = alloc_tree->insert(a);

    if (!record_snapshot)
        return;

    Caliper c = Caliper();

    // Record snapshot
    cali_id_t attrs[] = {
        cali_alloc_addr_attr_id,
        cali_alloc_elem_size_attr_id,
        cali_alloc_num_elems_attr_id,
        cali_alloc_total_size_attr_id
    };
    
    Variant data[] = {
        Variant(a->m_start_address),
        Variant(cali_make_variant_from_uint(a->m_elem_size)),
        Variant(cali_make_variant_from_uint(a->m_num_elems)),
        Variant(cali_make_variant_from_uint(a->m_bytes))
    };

    Node *string_nodes[] = {
        c.make_tree_entry(alloc_label_attr, Variant(CALI_TYPE_STRING, a->m_label.data(), a->m_label.size())),
        c.make_tree_entry(alloc_fn_attr, Variant(CALI_TYPE_STRING, fn_name.data(), fn_name.size()))
    };

    SnapshotRecord trigger_info(2, string_nodes, 4, attrs, data);
    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);
}

Allocation AllocTracker::remove_allocation(uint64_t address, 
                                           const std::string fn_name,
                                           bool record_snapshot) {
    Allocation removed = alloc_tree->remove(address);

    if (!record_snapshot)
        return removed;

    Caliper c = Caliper();
    
    Node *string_nodes[] = {
        c.make_tree_entry(alloc_label_attr, Variant(CALI_TYPE_STRING, removed.m_label.data(), removed.m_label.size())),
        c.make_tree_entry(alloc_fn_attr, Variant(CALI_TYPE_STRING, fn_name.data(), fn_name.size()))
    };

    SnapshotRecord trigger_info(2, string_nodes, 0, nullptr, nullptr);
    c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);

    return removed;
}

Allocation *AllocTracker::get_allocation_at(uint64_t address) {
    if (AllocTree::AllocNode *node = alloc_tree->get_allocation_at(address))
        return node->allocation;
    else
        return nullptr;
}

Allocation *AllocTracker::find_allocation_containing(uint64_t address) {
    if (AllocTree::AllocNode *node = alloc_tree->find_allocation_containing(address))
        return node->allocation;
    else
        return nullptr;
}

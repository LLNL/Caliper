#pragma once

#include <cstdlib>
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <numeric>

enum HAND {
    LEFT = -1,
    NA = 0,
    RIGHT = 1
};

class Allocation {

public:
    static size_t num_bytes(const size_t elem_size, const std::vector<size_t> &dimensions)
    {
       return std::accumulate(dimensions.begin(),
                              dimensions.end(),
                              elem_size,
                              std::multiplies<size_t>());
    }

public:
    Allocation(const std::string &label,
               const uint64_t start_address,
               const size_t elem_size,
               const std::vector<size_t> &dimensions)
        : m_label(label),
          m_start_address(start_address),
          m_elem_size(elem_size),
          m_dimensions(dimensions),
          m_bytes(num_bytes(elem_size, dimensions)),
          m_end_address(start_address + m_bytes)
    { }

    bool contains(uint64_t address)
    { return m_start_address <= address && m_end_address >= address; }


    const std::string           m_label;
    const uint64_t              m_start_address;
    const size_t                m_elem_size;
    const std::vector<size_t>   m_dimensions;
    const uint64_t              m_end_address;
    const size_t                m_bytes;
};

class AllocNode {

public:
    AllocNode(Allocation *allocation, AllocNode *parent, AllocNode *left, AllocNode*right, HAND handedness)
        : allocation(allocation),
          parent(parent),
          left(left),
          right(right),
          handedness(handedness),
          key(allocation->m_start_address)
    { }

    AllocNode* insert(Allocation *allocation);
    AllocNode* find_allocation_containing(uint64_t address);

    AllocNode* findMin();
    AllocNode* findMax();

    uint64_t key;

    AllocNode *parent;
    AllocNode *left;
    AllocNode *right;

    Allocation *allocation;

    HAND handedness;
};

class AllocTree {

public:
    explicit AllocTree(AllocNode *root = nullptr)
        : root(root)
    { }

    AllocNode *root;
    AllocNode* insert(Allocation *allocation);
    void remove(uint64_t start_address);
    Allocation* find_allocation_containing(uint64_t address);

    void splay(AllocNode *node);
    void rotate_left(AllocNode *node);
    void rotate_right(AllocNode *node);
};

class AllocTracker {

public:
    AllocTracker() = default;

    void add_allocation(const std::string &label,
                        const uint64_t addr,
                        const size_t elem_size,
                        const std::vector<size_t> &dimensions)
    {
        alloc_map[addr] = alloc_tree.insert(new Allocation(label, addr, elem_size, dimensions));
    }

    void remove_allocation(uint64_t address)
    {
        alloc_tree.remove(address);
        alloc_map.erase(address);
    }

    Allocation* get_allocation_at(uint64_t address)
    {
        return alloc_map[address]->allocation;
    }

    Allocation* find_allocation_containing(uint64_t address)
    {
        return alloc_tree.find_allocation_containing(address);
    }

private:
    AllocTree alloc_tree;
    std::map<uint64_t, AllocNode*> alloc_map;
};

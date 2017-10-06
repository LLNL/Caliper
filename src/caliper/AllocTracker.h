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
    static size_t num_bytes(const size_t elem_size,
                            const std::vector<size_t> &dimensions);

public:
    Allocation(const std::string &label,
               const uint64_t start_address,
               const size_t elem_size,
               const std::vector<size_t> &dimensions);
    ~Allocation();

    bool contains(uint64_t address);
    const size_t* index_of(uint64_t address);

    const std::string           m_label;
    const uint64_t              m_start_address;
    const size_t                m_elem_size;
    const std::vector<size_t>   m_dimensions;
    const uint64_t              m_end_address;
    const size_t                m_bytes;

    const size_t    m_num_dimensions;
    size_t*         m_index_ret;
};

class AllocNode {

public:
    AllocNode(Allocation *allocation,
              AllocNode *parent,
              AllocNode *left,
              AllocNode*right,
              HAND handedness);

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
                        const std::vector<size_t> &dimensions);

    void remove_allocation(uint64_t address);

    Allocation* get_allocation_at(uint64_t address);

    Allocation* find_allocation_containing(uint64_t address);

private:
    AllocTree alloc_tree;
    std::map<uint64_t, AllocNode*> alloc_map;
};

#pragma once

#include <cstdlib>
#include <cstdint>

enum HAND {
    LEFT = -1,
    NA = 0,
    RIGHT = 1
};

class Allocation {

public:
    Allocation(uint64_t id, uint64_t start_address, size_t bytes)
        : id(id), 
          start_address(start_address), 
          bytes(bytes),
          end_address(start_address + bytes)
    { }

    // Allocation(uint64_t id, uint64_t start_address, uint64_t end_address)
    //     : id(id), 
    //       start_address(start_address), 
    //       bytes(end_address - start_address),
    //       end_address(end_address)
    // { }

    bool contains(uint64_t address)
    { return start_address <= address && end_address >= address; }

    uint64_t id;
    uint64_t start_address;
    uint64_t end_address;
    size_t bytes;
};

class AllocNode {

public:
    AllocNode(Allocation *allocation, AllocNode *parent, AllocNode *left, AllocNode*right, HAND handedness)
        : allocation(allocation),
          parent(parent),
          left(left),
          right(right),
          handedness(handedness),
          key(allocation->start_address)
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
    AllocTree(AllocNode *root = nullptr)
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
    AllocTracker()
    { }

    void add_allocation(uint64_t id, uint64_t address, size_t bytes)
    { alloc_tree.insert(new Allocation(id, address, bytes)); }

    void remove_allocation(uint64_t address)
    { alloc_tree.remove(address); }

    Allocation* find_allocation_containing(uint64_t address)
    { return alloc_tree.find_allocation_containing(address); }

private:
    AllocTree alloc_tree;
};

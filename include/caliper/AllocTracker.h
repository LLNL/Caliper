#pragma once

#include <cstdlib>
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <numeric>
#include <memory>

namespace cali 
{
namespace DataTracker
{

class Allocation {

public:
    static size_t num_bytes(const size_t elem_size,
                                 const size_t dimensions[],
                                 const size_t num_dimensions);

    static size_t num_elems(const size_t dimensions[],
                                 const size_t num_dimensions);

public:
    Allocation(const std::string &label,
               const uint64_t start_address,
               const size_t elem_size,
               const size_t dimensions[],
               const size_t num_dimensions);
    ~Allocation();

    bool contains(uint64_t address);
    size_t index_1D(uint64_t address);
    const size_t* index_ND(uint64_t address);

    const std::string   m_label;
    const uint64_t      m_start_address;
    const size_t        m_elem_size;
    size_t*             m_dimensions;
    const size_t        m_num_elems;
    const size_t        m_bytes;
    const uint64_t      m_end_address;

    const size_t    m_num_dimensions;
    size_t*         m_index_ret;
};

class AllocTracker {

    struct AllocTree;
    std::shared_ptr<AllocTree> alloc_tree;

    // When the tree is being modified by add_ or remove_
    // do NOT trigger malloc hooks (how to tell AllocService??)
    //
    // If a sample is generated during modification, 
    // do NOT resolve the sample 
    //
    // Solution 1:
    //   Check if I can get a lock on the tree, if not, skip
    //      Problem: multiple threads allocating, many valid allocations skipped
    //
    // Solution 2:
    //   Spinlock until I can get a lock on the tree
    //      Problem: if sample is triggered while tree is locked, will deadlock
    //
    // Solution 3: 
    //   use unhooked malloc within the tree
    //   allow others to spinlock while waiting to modify the tree
    //   fail anyone who tries to look up the tree during modification

public:
    AllocTracker();

    void add_allocation(const std::string &label,
                        const uint64_t addr,
                        const size_t elem_size,
                        const size_t dimensions[],
                        const size_t num_dimensions);
    bool remove_allocation(uint64_t address);
    Allocation* get_allocation_at(uint64_t address);
    Allocation* find_allocation_containing(uint64_t address);

};
}
}

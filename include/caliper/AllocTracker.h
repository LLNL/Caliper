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
                            const std::vector<size_t> &dimensions);

public:
    Allocation(const std::string &label,
               const uint64_t start_address,
               const size_t elem_size,
               const std::vector<size_t> &dimensions);
    ~Allocation();

    bool contains(uint64_t address);
    const size_t index_1D(uint64_t address);
    const size_t* index_ND(uint64_t address);

    const std::string           m_label;
    const uint64_t              m_start_address;
    const size_t                m_elem_size;
    const std::vector<size_t>   m_dimensions;
    const uint64_t              m_end_address;
    const size_t                m_bytes;

    const size_t    m_num_dimensions;
    size_t*         m_index_ret;
};

class AllocTracker {

    struct AllocTree;
    std::shared_ptr<AllocTree> alloc_tree;

public:
    AllocTracker();

    void add_allocation(const std::string &label,
                        const uint64_t addr,
                        const size_t elem_size,
                        const std::vector<size_t> &dimensions);
    bool remove_allocation(uint64_t address);
    Allocation* get_allocation_at(uint64_t address);
    Allocation* find_allocation_containing(uint64_t address);

};
}
}

#pragma once

#include <cstdlib>
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <numeric>
#include <memory>
#include <atomic>

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
    Allocation(const Allocation &other);
    ~Allocation();

    bool isValid() {
        return m_bytes != 0;
    }

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

    static const Allocation invalid;
};

class AllocTracker {

    struct AllocTree;
    std::shared_ptr<AllocTree> alloc_tree;

    static const std::string cali_alloc;
    static const std::string cali_free;

    bool m_track_ranges;

public:
    AllocTracker(bool track_ranges = true);

    void set_track_ranges(bool track_ranges);
    uint64_t get_active_bytes();

    void add_allocation(const std::string &label,
                        const uint64_t addr,
                        const size_t elem_size,
                        const size_t dimensions[],
                        const size_t num_dimensions,
                        const std::string fn_name = cali_alloc,
                        bool record_snapshot = true,
                        bool track_range = true,
                        bool count_same_sized_allocs = false);
    Allocation remove_allocation(uint64_t address, 
                                 const std::string fn_name = cali_free,
                                 bool record_snapshot = true);
    Allocation* get_allocation_at(uint64_t address);
    Allocation* find_allocation_containing(uint64_t address);

};
}
}

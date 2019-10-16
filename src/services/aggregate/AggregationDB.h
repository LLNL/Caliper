// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#include "caliper/Caliper.h"

#include "caliper/common/Attribute.h"

#include <memory>
#include <vector>

namespace aggregate
{

struct StatisticsAttributes
{
    cali::Attribute min_attr;
    cali::Attribute max_attr;
    cali::Attribute sum_attr;
    cali::Attribute avg_attr;
};

struct AggregateAttributeInfo
{
    std::vector<cali::Attribute> key_attributes;
    std::vector<cali_id_t> key_attribute_ids;
    std::vector<cali::Attribute> aggr_attributes;
    std::vector<StatisticsAttributes> stats_attributes;
    cali::Attribute count_attribute;
};

//
// --- AggregateDB class
//

class AggregationDB
{
    struct AggregationDBImpl;
    std::unique_ptr<AggregationDBImpl> mP;

public:

    AggregationDB();
    
    ~AggregationDB();
    
    void   process_snapshot(const AggregateAttributeInfo&, cali::Caliper*, const cali::SnapshotRecord*);
    
    void   clear();
    size_t flush(const AggregateAttributeInfo&, cali::Caliper*, cali::SnapshotFlushFn);

    size_t num_trie_entries() const;
    size_t num_kernel_entries() const;
    size_t num_dropped() const;
    size_t num_skipped_keys() const;
    size_t max_keylen() const;
    size_t num_trie_blocks() const;
    size_t num_kernel_blocks() const;
    size_t bytes_reserved() const;

};

} // namespace aggregate

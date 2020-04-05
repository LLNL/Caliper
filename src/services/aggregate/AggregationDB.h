// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"

#include "caliper/common/Attribute.h"

#include <memory>
#include <vector>

#define CALI_AGG_HISTOGRAM_BINS      10

namespace aggregate
{

struct ResultAttributes
{
    cali::Attribute min_attr;
    cali::Attribute max_attr;
    cali::Attribute sum_attr;
    cali::Attribute avg_attr;
#ifdef CALIPER_ENABLE_HISTOGRAMS
    cali::Attribute histogram_attr[CALI_AGG_HISTOGRAM_BINS];
#endif
};

struct AttributeInfo
{
    std::vector<cali::Attribute>  ref_key_attrs;
    std::vector<cali::Attribute>  imm_key_attrs;

    std::vector<cali::Attribute>  aggr_attrs;

    std::vector<ResultAttributes> result_attrs;

    cali::Attribute               count_attr;
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
    
    void   process_snapshot(cali::Caliper*, const cali::SnapshotRecord*, const AttributeInfo&, bool implicit_grouping);
    
    void   clear();
    size_t flush(const AttributeInfo&, cali::Caliper*, cali::SnapshotFlushFn);

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

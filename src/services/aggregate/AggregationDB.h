// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#include "caliper/caliper-config.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Attribute.h"

#include <memory>
#include <vector>

#define CALI_AGG_HISTOGRAM_BINS 10

namespace aggregate
{

struct ResultAttributes {
    cali::Attribute min_attr;
    cali::Attribute max_attr;
    cali::Attribute sum_attr;
    cali::Attribute avg_attr;
#ifdef CALIPER_ENABLE_HISTOGRAMS
    cali::Attribute histogram_attr[CALI_AGG_HISTOGRAM_BINS];
#endif
};

struct AttributeInfo {
    std::vector<cali::Attribute> ref_key_attrs;
    std::vector<cali::Attribute> imm_key_attrs;

    std::vector<cali::Attribute> aggr_attrs;

    std::vector<ResultAttributes> result_attrs;

    cali::Attribute count_attr;
    cali::Attribute slot_attr;

    bool implicit_grouping;
    bool group_nested;
};

//
// --- AggregateDB class
//

class AggregationDB
{
    struct AggregationDBImpl;
    std::unique_ptr<AggregationDBImpl> mP;

public:

    AggregationDB(cali::Caliper* c);

    ~AggregationDB();

    void process_snapshot(cali::Caliper*, cali::SnapshotView, const AttributeInfo&);

    void   clear();
    size_t flush(const AttributeInfo&, cali::Caliper*, cali::SnapshotFlushFn);

    size_t num_dropped() const;
    size_t max_hash_len() const;
    size_t num_entries() const;
    size_t num_kernels() const;
    size_t bytes_reserved() const;
};

} // namespace aggregate

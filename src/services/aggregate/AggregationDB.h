// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#include "caliper/caliper-config.h"

#include <caliper/Caliper.h>
#include <caliper/SnapshotRecord.h>

#include <caliper/common/Attribute.h>

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
    std::vector<cali::Attribute>  imm_key_attrs;
    std::vector<cali::Attribute>  aggr_attrs;
    std::vector<ResultAttributes> result_attrs;
    cali::Attribute count_attr;
    cali::Attribute slot_attr;
};

struct AggregateKernel {
    cali::Variant min;
    cali::Variant max;
    cali::Variant sum;
    unsigned      count;

#ifdef CALIPER_ENABLE_HISTOGRAMS
    int histogram_max;
    int histogram[CALI_AGG_HISTOGRAM_BINS] = { 0 };

    // Quick way to get expoent out of a double.
    struct getExponent {
        union {
            double        val;
            std::uint16_t sh[4];
        };
    };
#endif

    AggregateKernel()
        : count(0)
#ifdef CALIPER_ENABLE_HISTOGRAMS
          ,
          histogram_max(0)
#endif
    {}

    inline void update(const cali::Variant& val)
    {
        cali::Variant::update_minmaxsum(val, min, max, sum);
        ++count;

#ifdef CALIPER_ENABLE_HISTOGRAMS
        //grab the shifted exponent from double, cast as int.
        std::uint64_t val_uint;
        std::memcpy(&val_uint, &val, 8);
        val_uint >>= 52;
        val_uint &= 0x7FF;
        //The bias for double is 1023, which means histogram
        //boundaries at 4x would lie at -0.5, 2.  To make things even
        //powers of 4 for ease of documentation, we need the bias to
        //be 1024.
        //making bins of size 4x, which means dividing exponent by 2.
        int exponent = (val_uint + 1) / 2;
        if (exponent > histogram_max) {
            //shift down values as necessary.
            int shift = std::min(exponent - histogram_max, CALI_AGG_HISTOGRAM_BINS - 1);
            for (int ii = 1; ii < shift + 1; ii++) {
                histogram[0] += histogram[ii];
            }
            for (int ii = shift + 1; ii < CALI_AGG_HISTOGRAM_BINS; ii++) {
                int jj        = ii - shift;
                histogram[jj] = histogram[ii];
            }
            for (int jj = CALI_AGG_HISTOGRAM_BINS - shift; jj < CALI_AGG_HISTOGRAM_BINS; jj++) {
                histogram[jj] = 0;
            }
            histogram_max = exponent;
        }
        int index = std::max(CALI_AGG_HISTOGRAM_BINS - 1 - (histogram_max - exponent), 0);
        histogram[index]++;
#endif
    }
};

struct AggregateEntry {
    size_t count;
    size_t key_idx;
    size_t key_len;
    size_t kernels_idx;
    size_t num_kernels;
    size_t next_entry_idx;
};

//
// --- AggregateDB class
//

class AggregationDB
{
    cali::Node m_aggr_root_node;

    size_t m_max_hash_len;

    std::vector<AggregateEntry>  m_entries;
    std::vector<cali::Entry>     m_keyents;
    std::vector<AggregateKernel> m_kernels;
    std::vector<size_t>          m_hashmap;

    AggregateEntry* find_or_create_entry(cali::SnapshotView key, std::size_t hash, std::size_t num_aggr_attrs, bool can_alloc);

public:

    explicit AggregationDB(cali::Caliper* c);

    ~AggregationDB();

    void process_snapshot(cali::Caliper*, cali::SnapshotView, const AttributeInfo&);

    void   clear();
    size_t flush(const AttributeInfo&, cali::Caliper*, cali::SnapshotFlushFn);

    size_t num_dropped() const { return m_entries[0].count; }
    size_t max_hash_len() const { return m_max_hash_len; };
    size_t num_entries() const { return m_entries.size(); };
    size_t num_kernels() const { return m_kernels.size(); };
    size_t bytes_reserved() const;
};

} // namespace aggregate

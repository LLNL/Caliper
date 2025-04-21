// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "AggregationDB.h"

#include "caliper/SnapshotRecord.h"

#include "caliper/common/Node.h"
#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

using namespace cali;
using namespace aggregate;

#define MAX_KEYLEN 16

AggregateEntry* AggregationDB::find_or_create_entry(SnapshotView key, std::size_t hash, std::size_t num_aggr_attrs, bool can_alloc)
{
    hash           = hash % m_hashmap.size();
    size_t key_len = key.size();
    size_t count   = 0;

    for (std::size_t idx = m_hashmap[hash]; idx != 0; idx = m_entries[idx].next_entry_idx) {
        AggregateEntry* e = &m_entries[idx];
        if (key_len == e->key_len && std::equal(key.begin(), key.end(), m_keyents.begin() + e->key_idx))
            return e;
        ++count;
    }

    // --- entry not found, check if we can create a new entry
    //

    if (!can_alloc) {
        if (m_kernels.size() + num_aggr_attrs >= m_kernels.capacity())
            return &m_entries[0];
        if (m_keyents.size() + key_len >= m_keyents.capacity())
            return &m_entries[0];
        if (m_entries.size() + 1 >= m_entries.capacity())
            return &m_entries[0];
    }

    size_t kernels_idx = m_kernels.size();
    m_kernels.resize(m_kernels.size() + num_aggr_attrs, AggregateKernel());

    size_t key_idx = m_keyents.size();
    std::copy(key.begin(), key.end(), std::back_inserter(m_keyents));

    AggregateEntry e;

    e.count          = 0;
    e.key_idx        = key_idx;
    e.key_len        = key_len;
    e.kernels_idx    = kernels_idx;
    e.num_kernels    = num_aggr_attrs;
    e.next_entry_idx = m_hashmap[hash];

    size_t entry_idx = m_entries.size();
    m_entries.push_back(e);
    m_hashmap[hash] = entry_idx;

    m_max_hash_len = std::max(m_max_hash_len, count + 1);

    return &m_entries[entry_idx];
}

void AggregationDB::process_snapshot(Caliper* c, SnapshotView rec, const AttributeInfo& info)
{
    if (rec.empty())
        return;

    // --- extract key entries

    FixedSizeSnapshotRecord<MAX_KEYLEN> key;
    std::size_t                         hash = 0;

    for (const Entry& e : rec)
        if (e.is_reference()) {
            key.builder().append(e);
            hash += e.node()->id();
        }

    for (const Attribute& attr : info.imm_key_attrs) {
        Entry e = rec.get_immediate_entry(attr);
        if (!e.empty()) {
            key.builder().append(e);
            hash += e.node()->id();
            hash += e.value().to_uint();
        }
    }

    AggregateEntry* entry = find_or_create_entry(key.view(), hash, info.aggr_attrs.size(), !c->is_signal());

    // --- update values

    ++entry->count;

    for (size_t a = 0; a < std::min(entry->num_kernels, info.aggr_attrs.size()); ++a) {
        Entry e = rec.get_immediate_entry(info.aggr_attrs[a]);

        if (e.empty())
            continue;

        m_kernels[entry->kernels_idx + a].update(e.value());
    }
}

void AggregationDB::clear()
{
    m_hashmap.assign(m_hashmap.size(), 0);
    m_entries.resize(1);
    m_kernels.resize(0);
    m_keyents.resize(0);

    m_entries[0].count = 0;
}

size_t AggregationDB::flush(const AttributeInfo& info, Caliper* c, SnapshotFlushFn proc_fn)
{
    size_t num_written = 0;

    for (const AggregateEntry& entry : m_entries) {
        if (entry.count == 0)
            continue;

        SnapshotView kv(entry.key_len, &m_keyents[entry.key_idx]);

        std::vector<Entry> rec;
        rec.reserve(kv.size() + 4 * entry.num_kernels + 2);

        std::copy(kv.begin(), kv.end(), std::back_inserter(rec));

        for (std::size_t a = 0; a < entry.num_kernels; ++a) {
            AggregateKernel* k = &m_kernels[entry.kernels_idx + a];

            if (k->count == 0)
                continue;

            rec.push_back(Entry(info.result_attrs[a].min_attr, k->min));
            rec.push_back(Entry(info.result_attrs[a].max_attr, k->max));
            rec.push_back(Entry(info.result_attrs[a].sum_attr, k->sum));
            rec.push_back(Entry(info.result_attrs[a].avg_attr, k->sum.div(k->count)));
#ifdef CALIPER_ENABLE_HISTOGRAMS
            for (int ii = 0; ii < CALI_AGG_HISTOGRAM_BINS; ii++) {
                rec.push_back(Entry(
                    info.stats_attributes[a].histogram_attr[ii],
                    Variant(cali_make_variant_from_uint(k->histogram[ii]))
                ));
            }
#endif
        }

        rec.push_back(Entry(info.count_attr, cali_make_variant_from_uint(entry.count)));
        rec.push_back(Entry(info.slot_attr, cali_make_variant_from_uint(num_written)));

        // --- write snapshot record
        proc_fn(*c, rec);
        ++num_written;
    }

    return num_written;
}

size_t AggregationDB::bytes_reserved() const
{
    return m_hashmap.capacity() * sizeof(size_t) + m_kernels.capacity() * sizeof(AggregateKernel)
           + m_keyents.capacity() * sizeof(Entry) + m_entries.capacity() * sizeof(AggregateEntry);
}

AggregationDB::AggregationDB(Caliper* c)
    : m_aggr_root_node(CALI_INV_ID, CALI_INV_ID, Variant()), m_max_hash_len(0)
{
    m_kernels.reserve(16384);
    m_keyents.reserve(16384);
    m_entries.reserve(4096);
    m_hashmap.assign(8192, static_cast<std::size_t>(0));

    Attribute attr =
        c->create_attribute("skipped.records", CALI_TYPE_STRING, CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);
    Node* node = c->make_tree_entry(attr, Variant("SKIPPED"), &m_aggr_root_node);

    m_keyents.push_back(Entry(node));

    AggregateEntry e;

    e.count          = 0;
    e.key_idx        = 0;
    e.key_len        = 1;
    e.kernels_idx    = 0;
    e.num_kernels    = 0;
    e.next_entry_idx = 0;

    m_entries.push_back(e);
}

AggregationDB::~AggregationDB()
{
}
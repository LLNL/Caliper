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

#define MAX_KEYLEN 20

namespace
{

struct AggregateKernel {
    double   min;
    double   max;
    double   sum;
    double   avg;
    int      count;

#ifdef CALIPER_ENABLE_HISTOGRAMS
    int histogram_max;
    int histogram[CALI_AGG_HISTOGRAM_BINS] = {0};

    // Quick way to get expoent out of a double.
    struct getExponent {
       union {
          double val;
          std::uint16_t sh[4];
       };
    };
#endif

    AggregateKernel()
        : min(std::numeric_limits<double>::max()),
          max(std::numeric_limits<double>::min()),
          sum(0), avg(0), count(0)
#ifdef CALIPER_ENABLE_HISTOGRAMS
          , histogram_max(0)
#endif
        { }

    void add(double val) {
        min  = std::min(min, val);
        max  = std::max(max, val);
        sum += val;
        avg  = ((count*avg) + val) / (count + 1.0);
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
        int exponent = (val_uint+1)/2;
        if (exponent > histogram_max) {
            //shift down values as necessary.
            int shift = std::min(exponent - histogram_max,CALI_AGG_HISTOGRAM_BINS-1);
            for (int ii=1; ii<shift+1; ii++) {
                histogram[0] += histogram[ii];
            }
            for (int ii=shift+1; ii<CALI_AGG_HISTOGRAM_BINS; ii++) {
                int jj = ii-shift;
                histogram[jj] = histogram[ii];
            }
            for (int jj=CALI_AGG_HISTOGRAM_BINS-shift; jj<CALI_AGG_HISTOGRAM_BINS; jj++) {
               histogram[jj] = 0;
            }
            histogram_max = exponent;
        }
        int index = std::max(CALI_AGG_HISTOGRAM_BINS-1 - (histogram_max-exponent), 0);
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

bool key_equal(SnapshotView lhs, SnapshotView rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (size_t i = 0; i < lhs.size(); ++i)
        if (lhs[i] != rhs[i])
            return false;

    return true;
}

} // namespace [anonymous]


struct AggregationDB::AggregationDBImpl
{
    Node                         m_aggr_root_node;

    size_t                       m_max_hash_len;

    std::vector<AggregateEntry>  m_entries;
    std::vector<Entry>           m_keyents;
    std::vector<AggregateKernel> m_kernels;
    std::vector<size_t>          m_hashmap;

    //
    // ---
    //

    Node* make_key_node(Caliper* c, SnapshotView rec, const std::vector<Attribute>& ref_key_attrs) {
        Node* key_node = &m_aggr_root_node;

        for (const Attribute& attr : ref_key_attrs) {
            Entry e = rec.get(attr);

            if (e.empty())
                continue;

            cali_id_t attr_id = attr.id();
            size_t count = 0;

            for (const Node* node = e.node(); node; node = node->parent())
                if (node->attribute() == attr_id)
                    ++count;

            const Node* *node_vec = static_cast<const Node**>(alloca(count * sizeof(const Node*)));
            memset(node_vec, 0, count * sizeof(const Node*));
            size_t n = count;

            for (const Node* node = e.node(); node; node = node->parent()) {
                if (node->attribute() == attr_id) {
                    node_vec[--n] = node;
                    if (n == 0)
                        break;
                }
            }

            key_node = c->make_tree_entry(count, node_vec, key_node);
        }

        return key_node == &m_aggr_root_node ? nullptr : key_node;
    }

    AggregateEntry* find_or_create_entry(SnapshotView key, std::size_t hash, std::size_t num_aggr_attrs, bool can_alloc) {
        hash = hash % m_hashmap.size();
        size_t count = 0;

        for (std::size_t idx = m_hashmap[hash]; idx != 0; idx = m_entries[idx].next_entry_idx) {
            AggregateEntry* e = &m_entries[idx];
            if (key_equal(key, SnapshotView(e->key_len, &m_keyents[e->key_idx])))
                return e;
            ++count;
        }

        // --- entry not found, check if we can create a new entry
        //

        if (!can_alloc) {
            if (m_kernels.size() + num_aggr_attrs >= m_kernels.capacity())
                return &m_entries[0];
            if (m_keyents.size() + key.size() >= m_keyents.capacity())
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
        e.key_len        = key.size();
        e.kernels_idx    = kernels_idx;
        e.num_kernels    = num_aggr_attrs;
        e.next_entry_idx = m_hashmap[hash];

        size_t entry_idx = m_entries.size();
        m_entries.push_back(e);
        m_hashmap[hash] = entry_idx;

        m_max_hash_len = std::max(m_max_hash_len, count+1);

        return &m_entries[entry_idx];
    }

    void process_snapshot(Caliper* c, SnapshotView rec, const AttributeInfo& info) {
        if (rec.empty())
            return;

        // --- extract key entries

        FixedSizeSnapshotRecord<MAX_KEYLEN> key;
        std::size_t hash = 0;

        if (info.implicit_grouping) {
            for (const Entry& e : rec)
                if (e.is_reference()) {
                    key.builder().append(e);
                    hash += e.node()->id();
                }
        } else {
            if (info.group_nested) {
                // exploit that nested attributes have their own entry
                for (const Entry& e : rec)
                    if (e.is_reference() && c->get_attribute(e.node()->attribute()).is_nested()) {
                        key.builder().append(e);
                        hash += e.node()->id();
                        break;
                    }
            }

            Node* node = make_key_node(c, rec, info.ref_key_attrs);
            if (node) {
                key.builder().append(Entry(node));
                hash += node->id();
            }
        }

        if (info.imm_key_attrs.size() > 0) {
            for (const Attribute& attr : info.imm_key_attrs) {
                Entry e = rec.get(attr);
                if (!e.empty()) {
                    key.builder().append(e);
                    hash += e.node()->id();
                    hash += e.value().to_uint();
                }
            }
        }

        AggregateEntry* entry = find_or_create_entry(key.view(), hash, info.aggr_attrs.size(), !c->is_signal());

        // --- update values

        ++entry->count;

        for (size_t a = 0; a < std::min(entry->num_kernels, info.aggr_attrs.size()); ++a) {
            Entry e = rec.get(info.aggr_attrs[a]);

            if (e.empty())
                continue;

            m_kernels[entry->kernels_idx + a].add(e.value().to_double());
        }
    }

    void clear() {
        m_hashmap.assign(m_hashmap.size(), 0);
        m_entries.resize(1);
        m_kernels.resize(0);
        m_keyents.resize(0);
        m_kernels.assign(m_entries[0].num_kernels, AggregateKernel());

        m_entries[0].count = 0;
    }

    size_t flush(const AttributeInfo& info, Caliper* c, SnapshotFlushFn proc_fn) {
        size_t num_written = 0;

        for (const AggregateEntry& entry : m_entries) {
            if (entry.count == 0)
                continue;

            SnapshotView kv(entry.key_len, &m_keyents[entry.key_idx]);

            std::vector<Entry> rec;
            rec.reserve(kv.size() + entry.num_kernels + 1);

            std::copy(kv.begin(), kv.end(), std::back_inserter(rec));

            for (std::size_t a = 0; a < entry.num_kernels; ++a) {
                AggregateKernel* k = &m_kernels[entry.kernels_idx + a];

                if (k->count == 0)
                    continue;

                rec.push_back(Entry(info.result_attrs[a].min_attr, Variant(k->min)));
                rec.push_back(Entry(info.result_attrs[a].max_attr, Variant(k->max)));
                rec.push_back(Entry(info.result_attrs[a].sum_attr, Variant(k->sum)));
                rec.push_back(Entry(info.result_attrs[a].avg_attr, Variant(k->avg)));
    #ifdef CALIPER_ENABLE_HISTOGRAMS
                for (int ii=0; ii<CALI_AGG_HISTOGRAM_BINS; ii++) {
                    rec.push_back(Entry(info.stats_attributes[a].histogram_attr[ii], Variant(cali_make_variant_from_uint(k->histogram[ii]))));
                }
    #endif
            }

            rec.push_back(Entry(info.count_attr, Variant(cali_make_variant_from_uint(entry.count))));

            // --- write snapshot record
            proc_fn(*c, rec);
            ++num_written;
        }

        return num_written;
    }

    AggregationDBImpl(Caliper* c, const AttributeInfo& info)
        : m_aggr_root_node(CALI_INV_ID, CALI_INV_ID, Variant()),
          m_max_hash_len(0)
        {
            m_kernels.reserve(16384);
            m_keyents.reserve(16384);
            m_entries.reserve(4096);
            m_hashmap.assign(8192, static_cast<std::size_t>(0));

            m_kernels.resize(info.aggr_attrs.size());

            Attribute attr =
                c->create_attribute("skipped.records", CALI_TYPE_STRING, CALI_ATTR_DEFAULT | CALI_ATTR_SKIP_EVENTS);
            Node* node =
                c->make_tree_entry(attr, Variant("SKIPPED"), &m_aggr_root_node);

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
};

//
// --- AggregationDB public interface
//

AggregationDB::AggregationDB(Caliper* c, const AttributeInfo& info)
    : mP(new AggregationDBImpl(c, info))
{ }

AggregationDB::~AggregationDB()
{
    mP.reset();
}

void
AggregationDB::process_snapshot(Caliper* c, SnapshotView rec, const AttributeInfo& info)
{
    mP->process_snapshot(c, rec, info);
}

void
AggregationDB::clear()
{
    mP->clear();
}

size_t
AggregationDB::flush(const AttributeInfo& info, Caliper* c, SnapshotFlushFn proc_fn)
{
    return mP->flush(info, c, proc_fn);
}

size_t
AggregationDB::num_dropped() const
{
    return mP->m_entries[0].count;
}

size_t
AggregationDB::max_hash_len() const
{
    return mP->m_max_hash_len;
}

size_t
AggregationDB::num_entries() const
{
    return mP->m_entries.size();
}

size_t
AggregationDB::num_kernels() const
{
    return mP->m_kernels.size();
}

size_t
AggregationDB::bytes_reserved() const
{
    return mP->m_hashmap.capacity() * sizeof(size_t) +
        mP->m_kernels.capacity() * sizeof(AggregateKernel) +
        mP->m_entries.capacity() * sizeof(AggregateEntry);
}

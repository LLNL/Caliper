// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "AggregationDB.h"

#include "caliper/SnapshotRecord.h"

#include "caliper/common/Node.h"
#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"

#include "caliper/common/c-util/vlenc.h"

#include <algorithm>
#include <cstring>
#include <limits>

using namespace cali;
using namespace aggregate;

#define MAX_KEYLEN          32

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

struct TrieNode {
    uint32_t next[256] = { 0 };
    uint32_t k_id      = 0xFFFFFFFF;
    uint32_t count     = 0;
};

template<typename T, size_t MAX_BLOCKS = 2048, size_t ENTRIES_PER_BLOCK = 1024>
class BlockAlloc {
    T*     m_blocks[MAX_BLOCKS] = { nullptr };
    size_t m_num_blocks;

public:

    T* get(size_t id, bool alloc) {
        size_t block = id / ENTRIES_PER_BLOCK;

        if (block >= MAX_BLOCKS)
            return nullptr;

        if (!m_blocks[block]) {
            if (alloc) {
                m_blocks[block] = new T[ENTRIES_PER_BLOCK];
                ++m_num_blocks;
            } else
                return nullptr;
        }

        return m_blocks[block] + (id % ENTRIES_PER_BLOCK);
    }

    void clear() {
        for (size_t b = 0; b < MAX_BLOCKS; ++b)
            delete[] m_blocks[b];

        std::fill_n(m_blocks, MAX_BLOCKS, nullptr);

        m_num_blocks = 0;
    }

    size_t num_blocks() const {
        return m_num_blocks;
    }

    BlockAlloc()
        : m_num_blocks(0)
        { }

    ~BlockAlloc() {
        clear();
    }
};

const cali::Node* get_key_node(Caliper* c, std::size_t n_ref, const cali::Node* const rec_nodes[], const std::vector<cali::Attribute>& ref_key_attrs, cali::Node* root)
{
    // --- count number of key entries
    std::size_t  key_entries = 0;
    const Node* *start_nodes = static_cast<const Node**>(alloca(n_ref * sizeof(const Node*)));

    memset(start_nodes, 0, n_ref * sizeof(const Node*));

    for (std::size_t i = 0; i < n_ref; ++i)
        for (const Node* node = rec_nodes[i]; node; node = node->parent())
            for (const Attribute& a : ref_key_attrs)
                if (a.id() == node->attribute()) {
                    ++key_entries;

                    // Save the snapshot nodes that lead to key nodes
                    // to short-cut the subsequent loop a bit
                    if (!start_nodes[i])
                        start_nodes[i] = node;
                }

    // FIXME: Strictly, we need to partition between nested/nonnested attribute nodes here
    //   and order nonnested ones by attribute ID, as nodes of different attributes
    //   can appear in any order in the context tree.

    // --- construct path of key nodes in reverse order, make/find new entry

    if (key_entries > 0) {
        const Node* *nodelist = static_cast<const Node**>(alloca(key_entries*sizeof(const Node*)));
        std::size_t  filled   = 0;

        memset(nodelist, 0, key_entries * sizeof(const Node*));

        for (std::size_t i = 0; i < n_ref; ++i)
            for (const Node* node = start_nodes[i]; node; node = node->parent())
                for (const Attribute& a : ref_key_attrs)
                    if (a.id() == node->attribute())
                        nodelist[key_entries - ++filled] = node;

        return c->make_tree_entry(key_entries, nodelist, root);
    }

    return root;
}

size_t pack_key(unsigned char* key,
                size_t n_key_nodes, const cali_id_t key_node_vec[],
                size_t n_imm,       const cali_id_t imm_attrs[], const Variant imm_data[],
                const std::vector<Attribute>& imm_key_attrs,
                size_t &skipped)
{
    // key encoding is as follows:
    //    - 1 u64: "toc" = 2 * num_nodes + (1 if immediate entries | 0 if no immediate entries)
    //    - num_nodes u64: key node ids
    //    - 1 u64: bitfield of indices into info.imm_key_attrs that mark immediate key entries
    //    - for each immediate entry, 1 u64 entry for the value

    // encode node key

    unsigned char   node_key[MAX_KEYLEN];
    size_t          node_key_len = 0;

    for (size_t i = 0; i < n_key_nodes; ++i) {
        unsigned char buf[16];
        size_t p = vlenc_u64(key_node_vec[i], buf);

        if (node_key_len + p + 1 >= MAX_KEYLEN) {
            ++skipped;
            break;
        }

        memcpy(node_key + node_key_len, buf, p);
        node_key_len += p;
    }

    // encode selected immediate key entries

    unsigned char   imm_key[MAX_KEYLEN];
    size_t          imm_key_len = 0;
    uint64_t        imm_key_bitfield = 0;

    for (size_t k = 0; k < imm_key_attrs.size(); ++k)
        for (size_t i = 0; i < n_imm; ++i)
            if (imm_key_attrs[k].id() == imm_attrs[i]) {
                unsigned char bbuf[16];
                unsigned char vbuf[16];
                size_t        p = vlenc_u64(*static_cast<const uint64_t*>(imm_data[i].data()), vbuf);

                // check size and discard entry if it won't fit :(
                if (node_key_len + imm_key_len + p + vlenc_u64(imm_key_bitfield | (1 << k), bbuf) + 1 >= MAX_KEYLEN) {
                    ++skipped;
                    break;
                }

                memcpy(imm_key + imm_key_len, vbuf, p);
                imm_key_bitfield |= (1 << k);
                imm_key_len      += p;
            }

    size_t pos = 0;

    pos += vlenc_u64(n_key_nodes * 2 + (imm_key_bitfield ? 1 : 0), key);
    memcpy(key+pos, node_key, node_key_len);
    pos += node_key_len;

    if (imm_key_bitfield) {
        pos += vlenc_u64(imm_key_bitfield, key+pos);
        memcpy(key+pos, imm_key, imm_key_len);
        pos += imm_key_len;
    }

    return pos;
}

} // namespace [anonymous]


struct AggregationDB::AggregationDBImpl
{
    BlockAlloc<TrieNode>        m_trie;
    BlockAlloc<AggregateKernel> m_kernels;

    Node                        m_aggr_root_node;

    // we maintain some internal statistics
    size_t                      m_num_trie_entries;
    size_t                      m_num_kernel_entries;
    size_t                      m_num_dropped;
    size_t                      m_num_skipped_keys;
    size_t                      m_max_keylen;

    //
    // --- functions
    //

    TrieNode* find_entry(size_t n, unsigned char* key, size_t num_ids, bool alloc) {
        TrieNode* entry = m_trie.get(0, alloc);

        for ( size_t i = 0; entry && i < n; ++i ) {
            uint32_t id = entry->next[key[i]];

            if (!id) {
                id = static_cast<uint32_t>(++m_num_trie_entries);
                entry->next[key[i]] = id;
            }

            entry = m_trie.get(id, alloc);
        }

        if (entry && entry->k_id == 0xFFFFFFFF) {
            if (num_ids > 0) {
                uint32_t first_id = static_cast<uint32_t>(m_num_kernel_entries + 1);

                m_num_kernel_entries += num_ids;

                for (unsigned i = 0; i < num_ids; ++i)
                    if (m_kernels.get(first_id + i, alloc) == 0)
                        return 0;

                entry->k_id = first_id;
            }
        }

        return entry;
    }

    void write_aggregated_snapshot(const unsigned char* key, const TrieNode* entry, const AttributeInfo& info, Caliper* c, SnapshotFlushFn proc_fn) {
        // --- decode key

        size_t    p = 0;

        uint64_t  toc = vldec_u64(key+p, &p); // first entry is 2*num_nodes + (1 : w/ immediate, 0 : w/o immediate)
        int       num_nodes = static_cast<int>(toc)/2;

        std::vector<Entry> rec;
        rec.reserve(num_nodes + info.imm_key_attrs.size() + 5);

        for (int i = 0; i < num_nodes; ++i)
            rec.push_back(Entry(c->node(vldec_u64(key + p, &p))));

        if (toc % 2 == 1) {
            // there are immediate key entries

            uint64_t imm_bitfield = vldec_u64(key+p, &p);

            for (size_t k = 0; k < info.imm_key_attrs.size(); ++k)
                if (imm_bitfield & (1 << k)) {
                    uint64_t val = vldec_u64(key+p, &p);
                    Variant  v(info.imm_key_attrs[k].type(), &val, sizeof(uint64_t));

                    rec.push_back(Entry(info.imm_key_attrs[k], v));
                }
        }

        // --- write aggregate entries

        int num_aggr_attr = info.aggr_attrs.size();

        for (int a = 0; a < num_aggr_attr; ++a) {
            AggregateKernel* k = m_kernels.get(entry->k_id+a, false);

            if (!k)
                break;
            if (k->count == 0)
                continue;

            rec.push_back(Entry(info.result_attrs[a].min_attr.id(), Variant(k->min)));
            rec.push_back(Entry(info.result_attrs[a].max_attr.id(), Variant(k->max)));
            rec.push_back(Entry(info.result_attrs[a].sum_attr.id(), Variant(k->sum)));
            rec.push_back(Entry(info.result_attrs[a].avg_attr.id(), Variant(k->avg)));
#ifdef CALIPER_ENABLE_HISTOGRAMS
            for (int ii=0; ii<CALI_AGG_HISTOGRAM_BINS; ii++) {
                rec.push_back(Entry(info.stats_attributes[a].histogram_attr[ii].id(), Variant(cali_make_variant_from_uint(k->histogram[ii]))));
            }
#endif
        }

        rec.push_back(Entry(info.count_attr, Variant(cali_make_variant_from_uint(entry->count))));

        // --- write snapshot record

        proc_fn(*c, rec);
    }

    size_t recursive_flush(size_t n, unsigned char* key, TrieNode* entry, const AttributeInfo& info, Caliper* c, SnapshotFlushFn proc_fn) {
        if (!entry)
            return 0;

        size_t num_written = 0;

        // --- write current entry if it represents a snapshot

        if (entry->count > 0)
            write_aggregated_snapshot(key, entry, info, c, proc_fn);

        num_written += (entry->count > 0 ? 1 : 0);

        // --- iterate over sub-records

        unsigned char* next_key = static_cast<unsigned char*>(alloca(n+2));

        memset(next_key, 0, n+2);
        memcpy(next_key, key, n);

        for (size_t i = 0; i < 256; ++i) {
            if (entry->next[i] == 0)
                continue;

            TrieNode* e  = m_trie.get(entry->next[i], false);
            next_key[n]  = static_cast<unsigned char>(i);

            num_written += recursive_flush(n+1, next_key, e, info, c, proc_fn);
        }

        return num_written;
    }

    void clear() {
        m_trie.clear();
        m_kernels.clear();

        m_num_trie_entries   = 0;
        m_num_kernel_entries = 0;
        m_num_dropped        = 0;
        m_num_skipped_keys   = 0;
        m_max_keylen         = 0;

        m_trie.get(0, true); // allocate the first block
        m_kernels.get(0, true);
    }

    void process_snapshot(Caliper* c, const SnapshotRecord* rec, const AttributeInfo& info, bool implicit_grouping) {
        auto n_ref = rec->size().n_nodes;
        auto n_imm = rec->size().n_immediate;

        if (n_ref + n_imm == 0)
            return;

        SnapshotRecord::Data data = rec->data();

        // --- extract refrence (node) key entries

        cali_id_t   key_node     = CALI_INV_ID;
        cali_id_t*  key_node_vec = &key_node;
        size_t      n_key_nodes  = 0;

        if (n_ref > 0) {
            if (implicit_grouping) {
                // --- implicit grouping: all nodes are key nodes

                key_node_vec = static_cast<cali_id_t*>(alloca((n_ref+1) * sizeof(cali_id_t)));
                n_key_nodes  = n_ref;

                for (size_t i = 0; i < n_ref; ++i)
                    key_node_vec[i] = data.node_entries[i]->id();

                // --- sort to make unique keys
                std::sort(key_node_vec, key_node_vec + n_key_nodes);
            } else if (!info.ref_key_attrs.empty()) {
                const Node* node = get_key_node(c, n_ref, data.node_entries, info.ref_key_attrs, &m_aggr_root_node);

                if (node != &m_aggr_root_node) {
                    if (node)
                        key_node_vec[n_key_nodes++] = node->id();
                    else
                        ++m_num_skipped_keys;
                }
            }
        }

        // --- encode key

        unsigned char key[MAX_KEYLEN];
        size_t len = pack_key(key, n_key_nodes, key_node_vec, n_imm, data.immediate_attr, data.immediate_data, info.imm_key_attrs, m_num_skipped_keys);

        m_max_keylen = std::max(len, m_max_keylen);

        // --- find entry

        TrieNode* entry = find_entry(len, key, info.aggr_attrs.size(), !c->is_signal());

        if (!entry) {
            ++m_num_dropped;
            return;
        }

        // --- update values

        ++entry->count;

        for (size_t a = 0; a < info.aggr_attrs.size(); ++a)
            for (size_t i = 0; i < n_imm; ++i)
                if (data.immediate_attr[i] == info.aggr_attrs[a].id()) {
                    AggregateKernel* k = m_kernels.get(entry->k_id + a, !c->is_signal());

                    if (k)
                        k->add(data.immediate_data[i].to_double());
                }
    }

    size_t flush(const AttributeInfo& info, Caliper* c, SnapshotFlushFn proc_fn) {
        TrieNode*     entry = m_trie.get(0, false);
        unsigned char key   = 0;

        return recursive_flush(0, &key, entry, info, c, proc_fn);
    }

    AggregationDBImpl()
        : m_aggr_root_node(CALI_INV_ID, CALI_INV_ID, Variant()),
          m_num_trie_entries(0),
          m_num_kernel_entries(0),
          m_num_dropped(0),
          m_num_skipped_keys(0),
          m_max_keylen(0)
        {
            Log(2).stream() << "Aggregate: creating aggregation database" << std::endl;

            // initialize first block
            m_trie.get(0, true);
            m_kernels.get(0, true);
        }
};

//
// --- AggregationDB public interface
//

AggregationDB::AggregationDB()
    : mP(new AggregationDBImpl)
{ }

AggregationDB::~AggregationDB()
{
    mP.reset();
}

void
AggregationDB::process_snapshot(Caliper* c, const SnapshotRecord* rec, const AttributeInfo& info, bool implicit_grouping)
{
    mP->process_snapshot(c, rec, info, implicit_grouping);
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
AggregationDB::num_trie_entries() const
{
    return mP->m_num_trie_entries;
}

size_t
AggregationDB::num_kernel_entries() const
{
    return mP->m_num_kernel_entries;
}

size_t
AggregationDB::num_dropped() const
{
    return mP->m_num_dropped;
}

size_t
AggregationDB::num_skipped_keys() const
{
    return mP->m_num_skipped_keys;
}

size_t
AggregationDB::max_keylen() const
{
    return mP->m_max_keylen;
}

size_t
AggregationDB::num_trie_blocks() const
{
    return mP->m_trie.num_blocks();
}

size_t
AggregationDB::num_kernel_blocks() const
{
    return mP->m_kernels.num_blocks();
}

size_t
AggregationDB::bytes_reserved() const
{
    return num_trie_blocks() * 1024 * sizeof(TrieNode) +
        num_kernel_blocks() * 1024 * sizeof(AggregateKernel);
}

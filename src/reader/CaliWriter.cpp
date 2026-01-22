// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// CsvWriter implementation

#include "caliper/reader/CaliWriter.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include "../common/util/format_util.h"

#include <mutex>
#include <set>

using namespace cali;

namespace
{

enum RecordKind { Snapshot, Globals };

void write_node_content(std::ostream& os, const cali::Node* node)
{
    util::write_uint64(os.write("__rec=node,id=", 14), node->id());
    util::write_uint64(os.write(",attr=", 6), node->attribute());

    node->data().write_cali(os.write(",data=", 6));

    if (node->parent() && node->parent()->id() != CALI_INV_ID)
        util::write_uint64(os.write(",parent=", 8), node->parent()->id());

    os.put('\n');
}

void write_record_content(
    std::ostream&             os,
    RecordKind                kind,
    const std::vector<Entry>& ref_entries,
    const std::vector<Entry>& imm_entries
)
{
    if (kind == RecordKind::Snapshot)
        os.write("__rec=ctx", 9);
    else if (kind == RecordKind::Globals)
        os.write("__rec=globals", 13);

    // write reference entries
    if (!ref_entries.empty()) {
        os.write(",ref", 4);
        for (const Entry& e : ref_entries)
            util::write_uint64(os.put('='), e.node()->id());
    }

    // write immediate entries
    if (!imm_entries.empty()) {
        os.write(",attr", 5);
        for (const Entry& e : imm_entries)
            util::write_uint64(os.put('='), e.attribute());

        os.write(",data", 5);
        for (const Entry& e : imm_entries)
            e.value().write_cali(os.put('='));
    }

    os.put('\n');
}

} // namespace

struct CaliWriter::CaliWriterImpl {
    OutputStream m_os;
    std::mutex   m_os_lock;

    std::set<cali_id_t> m_written_nodes;
    std::mutex          m_written_nodes_lock;

    std::size_t m_num_written;

    CaliWriterImpl(OutputStream& os) : m_os(os), m_num_written(0) {}

    void recursive_write_node(const CaliperMetadataAccessInterface& db, cali_id_t id)
    {
        if (id < 11) // don't write the hard-coded metadata nodes
            return;

        {
            std::lock_guard<std::mutex> g(m_written_nodes_lock);

            if (m_written_nodes.count(id) > 0)
                return;
        }

        Node* node = db.node(id);

        if (!node)
            return;

        recursive_write_node(db, node->attribute());

        Node* parent = node->parent();

        if (parent && parent->id() != CALI_INV_ID)
            recursive_write_node(db, parent->id());

        {
            std::lock_guard<std::mutex> g(m_os_lock);

            std::ostream* real_os = m_os.stream();

            ::write_node_content(*real_os, node);
            ++m_num_written;
        }

        {
            std::lock_guard<std::mutex> g(m_written_nodes_lock);

            m_written_nodes.insert(id);
        }
    }

    void write_entrylist(const CaliperMetadataAccessInterface& db, RecordKind kind, const std::vector<Entry>& rec)
    {
        // write node entries

        std::vector<Entry> ref_entries;
        std::vector<Entry> imm_entries;
        ref_entries.reserve(rec.size());
        imm_entries.reserve(rec.size());

        for (const Entry& e : rec) {
            if (e.is_reference()) {
                recursive_write_node(db, e.node()->id());
                ref_entries.push_back(e);
            } else if (e.is_immediate()) {
                recursive_write_node(db, e.attribute());
                imm_entries.push_back(e);
            }
        }

        // write the record

        {
            std::lock_guard<std::mutex> g(m_os_lock);

            std::ostream* real_os = m_os.stream();

            ::write_record_content(*real_os, kind, ref_entries, imm_entries);
            ++m_num_written;
        }
    }
};

CaliWriter::CaliWriter(OutputStream& os) : mP(new CaliWriterImpl(os))
{}

size_t CaliWriter::num_written() const
{
    return mP ? mP->m_num_written : 0;
}

void CaliWriter::write_snapshot(const CaliperMetadataAccessInterface& db, const std::vector<Entry>& list)
{
    mP->write_entrylist(db, ::RecordKind::Snapshot, list);
}

void CaliWriter::write_globals(const CaliperMetadataAccessInterface& db, const std::vector<Entry>& list)
{
    mP->write_entrylist(db, ::RecordKind::Globals, list);
}

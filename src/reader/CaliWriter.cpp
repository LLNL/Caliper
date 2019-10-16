// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
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

const char* esc_chars { "\\,=\n" }; ///< characters that need to be escaped

void write_node_content(std::ostream& os, const cali::Node* node)
{
    os << "__rec=node,id=" << node->id()
       << ",attr="         << node->attribute();

    util::write_esc_string(os << ",data=", node->data().to_string(), esc_chars);

    if (node->parent() && node->parent()->id() != CALI_INV_ID)
        os << ",parent=" << node->parent()->id();

    os << '\n';
}

void write_record_content(std::ostream& os, const char* record_type, int nr, int ni, const std::vector<Entry>& rec) {
    os << "__rec=" << record_type;
            
    // write reference entries

    if (nr > 0) {
        os << ",ref";
            
        for (const Entry& e : rec)
            if (e.is_reference())
                os << '=' << e.node()->id();
    }

    // write immediate entries

    if (ni > 0) {
        os << ",attr";

        for (const Entry& e : rec)
            if (e.is_immediate())
                os << '=' << e.attribute();

        os << ",data";

        for (const Entry& e : rec)
            if (e.is_immediate())
                util::write_esc_string(os << '=', e.value().to_string(), esc_chars);
    }

    os << '\n';
}

} // namespace [anonymous]

struct CaliWriter::CaliWriterImpl
{
    OutputStream  m_os;
    std::mutex    m_os_lock;

    std::set<cali_id_t> m_written_nodes;
    std::mutex    m_written_nodes_lock;

    std::size_t   m_num_written;


    CaliWriterImpl(OutputStream& os)
        : m_os(os),
          m_num_written(0)
    { }
    
    void recursive_write_node(const CaliperMetadataAccessInterface& db, cali_id_t id)
    {
        if (id < 11) // don't write the hard-coded metadata nodes
            return;

        {
            std::lock_guard<std::mutex>
                g(m_written_nodes_lock);

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
            std::lock_guard<std::mutex>
                g(m_os_lock);

            std::ostream* real_os = m_os.stream();
            
            ::write_node_content(*real_os, node);
            ++m_num_written;
        }

        {
            std::lock_guard<std::mutex>
                g(m_written_nodes_lock);

            if (m_written_nodes.count(id) > 0)
                return;
       
            m_written_nodes.insert(id);
        }
    }

    void write_entrylist(const CaliperMetadataAccessInterface& db,
                         const char* record_type,
                         const std::vector<Entry>& rec)
    {
        // write node entries; count the number of ref and immediate entries
        
        int nr = 0;
        int ni = 0;
        
        for (const Entry& e : rec) {
            if (e.is_reference()) {
                recursive_write_node(db, e.node()->id());
                ++nr;
            } else if (e.is_immediate()) {
                recursive_write_node(db, e.attribute());
                ++ni;
            }
        }

        // write the record
        
        {
            std::lock_guard<std::mutex>
                g(m_os_lock);

            std::ostream* real_os = m_os.stream();

            ::write_record_content(*real_os, record_type, nr, ni, rec);
            ++m_num_written;
        }
    }
};


CaliWriter::CaliWriter(OutputStream& os)
    : mP(new CaliWriterImpl(os))
{ }

CaliWriter::~CaliWriter()
{
    mP.reset();
}

size_t CaliWriter::num_written() const
{
    return mP ? mP->m_num_written : 0;
}

void CaliWriter::write_snapshot(const CaliperMetadataAccessInterface& db, const std::vector<Entry>& list)
{
    mP->write_entrylist(db, "ctx", list);
}

void CaliWriter::write_globals(const CaliperMetadataAccessInterface& db, const std::vector<Entry>& list)
{
    mP->write_entrylist(db, "globals", list);
}

// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file CsvWriter.cpp
/// CsvWriter implementation

#include "caliper/common/csv/CsvWriter.h"

#include "caliper/common/csv/CsvSpec.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/ContextRecord.h"
#include "caliper/common/Node.h"

#include <mutex>
#include <set>

using namespace cali;


struct CsvWriter::CsvWriterImpl
{
    std::ostream& m_os;
    std::mutex    m_os_lock;

    std::set<cali_id_t> m_written_nodes;
    std::mutex    m_written_nodes_lock;

    std::size_t   m_num_written;

    CsvWriterImpl(std::ostream& os)
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
            
            CsvSpec::write_record(m_os, node->record());
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

    void write_snapshot(const CaliperMetadataAccessInterface& db,
                        size_t n_nodes, const cali_id_t nodes[],
                        size_t n_imm,   const cali_id_t attr[], const Variant vals[])
    {
        int nn = std::min(static_cast<int>(n_nodes), 128);
        int ni = std::min(static_cast<int>(n_imm),   128);

        Variant v_node[128];
        Variant v_attr[128];

        for (int i = 0; i < nn; ++i) {
            v_node[i] = Variant(nodes[i]);
            recursive_write_node(db, nodes[i]);
        }
        for (int i = 0; i < ni;   ++i) {
            v_attr[i] = Variant(attr[i]);
            recursive_write_node(db, attr[i]);
        }

        int               n[3] = { nn,     ni,     ni   };
        const Variant* data[3] = { v_node, v_attr, vals };

        {
            std::lock_guard<std::mutex>
                g(m_os_lock);
            
            CsvSpec::write_record(m_os, ContextRecord::record_descriptor(), n, data);
            ++m_num_written;
        }
    }
};


CsvWriter::CsvWriter(std::ostream& os)
    : mP(new CsvWriterImpl(os))
{ }

CsvWriter::~CsvWriter()
{
    mP.reset();
}

size_t CsvWriter::num_written() const
{
    return mP->m_num_written;
}

void CsvWriter::write_snapshot(const CaliperMetadataAccessInterface& db,
                               size_t n_nodes, const cali_id_t nodes[],
                               size_t n_imm,   const cali_id_t attr[], const Variant vals[])
{
    mP->write_snapshot(db, n_nodes, nodes, n_imm, attr, vals);
}

void CsvWriter::operator()(const CaliperMetadataAccessInterface& db, const Node* node)
{
    mP->recursive_write_node(db, node->id());
}

void CsvWriter::operator()(const CaliperMetadataAccessInterface& db, const std::vector<Entry>& list)
{
    Variant v_node[128];
    Variant v_attr[128];
    Variant v_data[128];

    int nn = 0;
    int ni = 0;

    for (const Entry& e : list)
        if (e.node()) {
            if (nn > 127)
                continue;
            
            mP->recursive_write_node(db, e.node()->id());
            v_node[nn] = Variant(e.node()->id());

            ++nn;
        } else if (e.is_immediate()) {
            if (ni > 127)
                continue;
            
            mP->recursive_write_node(db, e.attribute());
            v_attr[ni] = Variant(e.attribute());
            v_data[ni] = e.value();

            ++ni;
        }

    int               n[3] = { nn,     ni,     ni     };
    const Variant* data[3] = { v_node, v_attr, v_data };

    {
        std::lock_guard<std::mutex>
            g(mP->m_os_lock);
            
        CsvSpec::write_record(mP->m_os, ContextRecord::record_descriptor(), n, data);
    }
}

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

#include "AttributeExtract.h"

#include "CaliperMetadataAccessInterface.h"
#include "Node.h"

using namespace cali;

struct AttributeExtract::AttributeExtractImpl
{
    SnapshotProcessFn      m_snap_fn;
    Attribute              m_id_attr;

    static const cali_id_t s_attr_id; // The "attribute" attribute id

    void process_node(CaliperMetadataAccessInterface& db, const Node* node) {
        if (node->attribute() != s_attr_id)
            return;

        if (m_id_attr == Attribute::invalid)
            m_id_attr = db.create_attribute("attribute.id", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

        EntryList list { Entry(node), Entry(m_id_attr, node->id()) };

        m_snap_fn(db, list);
    }

    AttributeExtractImpl(SnapshotProcessFn snap_fn)
        : m_snap_fn(snap_fn),
          m_id_attr(Attribute::invalid)
        { }
};

const cali_id_t AttributeExtract::AttributeExtractImpl::s_attr_id = 8;

AttributeExtract::AttributeExtract(SnapshotProcessFn snap_fn)
    : mP { new AttributeExtractImpl(snap_fn) } 
{ }

AttributeExtract::~AttributeExtract()
{
    mP.reset();
}

void AttributeExtract::operator()(CaliperMetadataAccessInterface& db, const Node* node)
{
    mP->process_node(db, node);
}

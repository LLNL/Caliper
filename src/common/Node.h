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

/** 
 * @file node.h 
 * Node class declaration
 */

#ifndef CALI_NODE_H
#define CALI_NODE_H

#include "cali_types.h"

#include "IdType.h"
#include "Record.h"
#include "RecordMap.h"
#include "Variant.h"

#include "util/lockfree-tree.hpp"

#include <atomic>

namespace cali 
{

//
// --- Node base class 
//

class Node : public IdType, public util::LockfreeIntrusiveTree<Node> 
{
    util::LockfreeIntrusiveTree<Node>::Node m_treenode;

    cali_id_t         m_attribute;
    Variant           m_data;

    std::atomic<bool> m_written; // temporary implementation - will go away

    static const RecordDescriptor s_record;

public:

    Node(cali_id_t id, cali_id_t attr, const Variant& data)
        : IdType(id),
          util::LockfreeIntrusiveTree<Node>(this, &Node::m_treenode), 
        m_attribute { attr }, m_data { data }, m_written { false }
        { }

    Node(const Node&) = delete;

    Node& operator = (const Node&) = delete;

    ~Node();

    bool equals(cali_id_t attr, const Variant& v) const {
        return m_attribute == attr ? m_data == v : false;
    }

    bool equals(cali_id_t attr, const void* data, size_t size) const;

    cali_id_t attribute() const { return m_attribute; }
    Variant   data() const      { return m_data;      }    

    // Temporary implementation - will go away
    bool      written() const   { return m_written.load(); }
    bool      check_written()   { return m_written.exchange(true); }
    void      write_path(WriteRecordFn recfn);
    
    /// @name Serialization API
    /// @{

    void      push_record(WriteRecordFn recfn) const;
    
    static const RecordDescriptor& record_descriptor() { return s_record; }

    RecordMap record() const;
};

} // namespace cali

#endif

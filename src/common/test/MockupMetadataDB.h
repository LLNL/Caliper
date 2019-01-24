// Copyright (c) 2016, Lawrence Livermore National Security, LLC.  
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

/// \file MockupMetadataDB.h 
/// Mockup Caliper metadata access interface for testing

#pragma once

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"

#include <gtest/gtest.h>

#include <map>

namespace cali
{

/// \brief A simple mockup implementation for CaliperMetadataAccessInterface
class MockupMetadataDB : public CaliperMetadataAccessInterface
{
    std::map<cali_id_t,   Node*>     m_node_map;
    std::map<cali_id_t,   Attribute> m_attr_map;
    std::map<std::string, Attribute> m_attr_names;

    // gtest FAIL() can only be called in void functions
    void fail(const char* str) {
        FAIL() << str;
    }
    
public:

    MockupMetadataDB()
    { }

    ~MockupMetadataDB()
    { }

    inline void
    add_node(Node* node) {
        m_node_map.insert(std::make_pair(node->id(), node));
    }

    inline void
    add_nodes(size_t n, Node* nodes[]) {
        for (size_t i = 0; i < n; ++i)
            add_node(nodes[i]);
    }
    
    inline void
    add_attribute(const Attribute& attr) {
        m_attr_map.insert(std::make_pair(attr.id(), attr));
        m_attr_names.insert(std::make_pair(attr.name(), attr));
    }
    
    inline Node*
    node(cali_id_t id) const {
        auto it = m_node_map.find(id);
        return it == m_node_map.end() ? nullptr : it->second;
    }

    inline Attribute
    get_attribute(cali_id_t id) const {
        auto it = m_attr_map.find(id);
        return it == m_attr_map.end() ? Attribute::invalid : it->second;
    }

    inline Attribute
    get_attribute(const std::string& name) const {
        auto it = m_attr_names.find(name);
        return it == m_attr_names.end() ? Attribute::invalid : it->second;
    }

    inline std::vector<Attribute>
    get_all_attributes() const {
        std::vector<Attribute> vec;

        for (auto p : m_attr_map)
            vec.push_back(p.second);

        return vec;
    }

    Attribute
    create_attribute(const std::string& name,
                     cali_attr_type     type,
                     int                prop = CALI_ATTR_DEFAULT,
                     int                meta = 0,
                     const Attribute*   meta_attr = nullptr,
                     const Variant*     meta_data = nullptr) {
        fail("create_attribute() is not implemented in MockupMetadataDB!");
        return Attribute::invalid;
    }

    Node*
    make_tree_entry(std::size_t n, const Node* nodelist[], Node* parent) {
        fail("make_tree_entry() is not implemented in MockupMetadataDB!");
        return nullptr;
    }

    inline std::vector<Entry>
    get_globals() {
        fail("get_globals() is not implemented in MockupMetadataDB!");
        return std::vector<Entry>();
    }
};

} // namespace

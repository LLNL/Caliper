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

#pragma once

#include "caliper/Caliper.h"

#include "caliper/common/Attribute.h"

#include <memory>
#include <vector>

namespace aggregate
{

struct StatisticsAttributes
{
    cali::Attribute min_attr;
    cali::Attribute max_attr;
    cali::Attribute sum_attr;
    cali::Attribute avg_attr;
};

struct AggregateAttributeInfo
{
    std::vector<cali::Attribute> key_attributes;
    std::vector<cali_id_t> key_attribute_ids;
    std::vector<cali::Attribute> aggr_attributes;
    std::vector<StatisticsAttributes> stats_attributes;
    cali::Attribute count_attribute;
};

//
// --- AggregateDB class
//

class AggregationDB
{
    struct AggregationDBImpl;
    std::unique_ptr<AggregationDBImpl> mP;

public:

    AggregationDB();
    
    ~AggregationDB();
    
    void   process_snapshot(const AggregateAttributeInfo&, cali::Caliper*, const cali::SnapshotRecord*);
    
    void   clear();
    size_t flush(const AggregateAttributeInfo&, cali::Caliper*, cali::SnapshotFlushFn);

    size_t num_trie_entries() const;
    size_t num_kernel_entries() const;
    size_t num_dropped() const;
    size_t num_skipped_keys() const;
    size_t max_keylen() const;
    size_t num_trie_blocks() const;
    size_t num_kernel_blocks() const;
    size_t bytes_reserved() const;

};

} // namespace aggregate

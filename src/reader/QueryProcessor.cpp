// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
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

#include "caliper/reader/QueryProcessor.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

using namespace cali;

struct QueryProcessor::QueryProcessorImpl
{
    Aggregator        aggregator;
    RecordSelector    filter;
    FormatProcessor   formatter;

    bool              do_aggregate;

    void
    process_record(CaliperMetadataAccessInterface& db, const EntryList& rec) {
        if (filter.pass(db, rec)) {
            if (do_aggregate)
                aggregator.add(db, rec);
            else
                formatter.process_record(db, rec);
        }
    }

    void
    flush(CaliperMetadataAccessInterface& db) {
        aggregator.flush(db, formatter);
        formatter.flush(db);
    }
    
    QueryProcessorImpl(const QuerySpec& spec, OutputStream& stream)
        : aggregator(spec),
          filter(spec),
          formatter(spec, stream)
    {
        do_aggregate = (spec.aggregation_ops.selection != QuerySpec::AggregationSelection::None);
    }
};


QueryProcessor::QueryProcessor(const QuerySpec& spec, OutputStream& stream)
    : mP(new QueryProcessorImpl(spec, stream))
{ }

QueryProcessor::~QueryProcessor()
{
    mP.reset();
}

void
QueryProcessor::process_record(CaliperMetadataAccessInterface& db, const EntryList& rec)
{
    
    mP->process_record(db, rec);
}

void
QueryProcessor::flush(CaliperMetadataAccessInterface& db)
{
    mP->flush(db);
}

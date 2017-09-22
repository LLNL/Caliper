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

/// \file FormatProcessor.h
/// FormatProcessor class

#pragma once

#include "QuerySpec.h"
#include "RecordProcessor.h"

#include <iostream>
#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;
class OutputStream;

/// \brief Format output based on a given query specification.
///   Essentially a factory for Caliper's output formatters.
/// \ingroup ReaderAPI
class FormatProcessor
{
    struct FormatProcessorImpl;
    std::shared_ptr<FormatProcessorImpl> mP;
    
public:

    /// \brief Create formatter for given query spec and output stream.
    FormatProcessor(const QuerySpec&, OutputStream&);

    ~FormatProcessor();

    /// \brief Add snapshot record to formatter. 
    void process_record(CaliperMetadataAccessInterface&, const EntryList&);

    /// \brief Flush formatter contents.
    ///
    /// There are two types of formatters: \e Stream formatters
    /// (such as csv or Expand) write each record directly into the output
    /// stream. In this case, flush does nothing. \e Buffered formatters
    /// (such as TableFormatter or TreeFormatter) need to read in all
    /// records before they can print output. In this case, flush triggers
    /// the actual output, and writes it to the given OutputStream.
    void flush(CaliperMetadataAccessInterface& db);

    /// \brief Make FormatProcessor usable as a SnapshotProcessFn.
    void operator()(CaliperMetadataAccessInterface& db, const EntryList& rec) {
        process_record(db, rec);
    }

    /// \brief Return all known formatter signatures.
    static const QuerySpec::FunctionSignature* formatter_defs();
};

}

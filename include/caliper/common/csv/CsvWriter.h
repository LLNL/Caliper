// Copyright (c) 2015-2017, Lawrence Livermore National Security, LLC.  
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

/// \file CsvWriter.h
/// \brief CsvWriter implementation

#pragma once

#include "../Entry.h"

#include <memory>
#include <vector>

namespace cali
{

class CaliperMetadataAccessInterface;
class Node;
class OutputStream;
    
class CsvWriter
{
    struct CsvWriterImpl;
    std::shared_ptr<CsvWriterImpl> mP;

public:

    CsvWriter()
    { }
    
    CsvWriter(OutputStream& os);

    ~CsvWriter();

    size_t num_written() const;
    
    void write_snapshot(const CaliperMetadataAccessInterface& db,
                        size_t n_nodes, const cali_id_t nodes[],
                        size_t n_imm,   const cali_id_t attr[], const Variant vals[]);
    
    void write_snapshot(const CaliperMetadataAccessInterface& db,
                        const std::vector<Entry>&);

    void write_globals(const CaliperMetadataAccessInterface& db,
                       const std::vector<Entry>&);

    void operator()(const CaliperMetadataAccessInterface&, const Node*);
    void operator()(const CaliperMetadataAccessInterface&, const std::vector<Entry>&);
};

}

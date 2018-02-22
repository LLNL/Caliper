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

/// \file query_common.h
/// \brief Common functionality for cali-query and mpi-caliquery

#pragma once

#include "caliper/reader/QuerySpec.h"
#include "caliper/reader/RecordProcessor.h"

namespace util
{

class Args;

}

namespace cali
{

class CaliperMetadataAccessInterface;

/// \brief Create QuerySpec from command-line arguments
class QueryArgsParser {
    bool        m_error;
    std::string m_error_msg;
    QuerySpec   m_spec;

public:

    QueryArgsParser()
        : m_error(true),
          m_error_msg("query not read")
    { }

    /// \brief Get query spec from cali-query command-line arguments.
    /// \return Returns \a true if successful, `false` in case of error.
    bool parse_args(const util::Args& args);

    bool        error() const     { return m_error;     }
    std::string error_msg() const { return m_error_msg; }

    QuerySpec   spec() const      { return m_spec;      }
};

/// \class SnapshotFilterStep
/// \brief Basically the chain link in the processing chain.
///    Passes result of \a m_filter_fn to \a m_push_fn.
struct SnapshotFilterStep {
    SnapshotFilterFn  m_filter_fn;  ///< This processing step
    SnapshotProcessFn m_push_fn;    ///< Next processing step

    SnapshotFilterStep(SnapshotFilterFn filter_fn, SnapshotProcessFn push_fn) 
        : m_filter_fn { filter_fn }, m_push_fn { push_fn }
    { }

    void operator ()(CaliperMetadataAccessInterface& db, const EntryList& list) {
        m_filter_fn(db, list, m_push_fn);
    }
};

}

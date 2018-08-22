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

/// \file OutputStream.h
/// Manage output streams

#pragma once

#include "Entry.h"

#include <iostream>
#include <memory>
#include <vector>

namespace cali
{

class CaliperMetadataAccessInterface;

/// \class OutputStream
/// \brief A simple stream abstraction class.
///   Handles file streams/stdout/stderr.
class OutputStream
{
    struct OutputStreamImpl;
    std::shared_ptr<OutputStreamImpl> mP;
    
public:
    
    enum StreamType {
        None,
        StdOut,
        StdErr,
        File
    };

    /// \brief Returns \a true if the stream is initialized, otherwise
    ///   returns \a false
    operator bool() const; 

    StreamType    type() const;

    /// \brief Return a C++ ostream. Opens/creates the underlying file stream
    ///   if needed.
    std::ostream& stream();

    OutputStream();

    ~OutputStream();

    /// \brief Set stream type. (Note: for file streams, use \a set_filename).
    void
    set_stream(StreamType type);

    /// \brief Set stream's file name to \a filename
    void
    set_filename(const char* filename);

    /// \brief Create stream's filename from the given format string pattern and
    ///   entry list.
    ///
    /// The filename is created from the format string \a formatstr.
    /// The format string can include attribute names enclosed with '%',
    /// (i.e., "%attribute.name%"). These fields will be replaced with the value
    /// attribute in the given record \a rec.
    ///
    /// For example, the format string "out-%mpi.rank%.txt" will result in a
    // file name like "out-0.txt" using the \a mpi.rank value in \a rec.
    ///
    /// The special values "stdout" and "stderr" for \a formatstr will redirect
    /// output to standard out and standard error, respectively.
    void
    set_filename(const char* formatstr,
                 const CaliperMetadataAccessInterface& db,
                 const std::vector<Entry>& rec);
};
    
}

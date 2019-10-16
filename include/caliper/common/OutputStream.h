// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

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
        File,
        User
    };

    /// \brief Returns \a true if the stream is initialized, otherwise
    ///   returns \a false
    operator bool() const; 

    StreamType    type() const;

    /// \brief Return a C++ ostream. Opens/creates the underlying file stream
    ///   if needed.
    std::ostream* stream();

    OutputStream();

    ~OutputStream();

    /// \brief Set stream type. (Note: for file streams, use \a set_filename).
    void
    set_stream(StreamType type);

    /// \brief Assign a user-given stream
    void
    set_stream(std::ostream* os);

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

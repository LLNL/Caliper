// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

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

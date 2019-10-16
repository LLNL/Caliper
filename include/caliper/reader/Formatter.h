// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Formatter.h
/// Output formatter base class

#pragma once

#include "RecordProcessor.h"

namespace cali
{

class CaliperMetadataAccessInterface;

/// \class Formatter
/// \brief Abstract base class for output formatters.
/// \ingroup ReaderAPI
    
class Formatter
{
public:

    virtual ~Formatter() { }

    /// Process a snapshot record.
    virtual void process_record(CaliperMetadataAccessInterface&, const EntryList&) = 0;

    /// Flush processed contents to stream. Need not be implemented for stream formatters.
    virtual void flush(CaliperMetadataAccessInterface&, std::ostream&)
    { }
};

} // namespace cali

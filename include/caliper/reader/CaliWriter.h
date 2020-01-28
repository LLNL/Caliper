// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file CaliWriter.h
/// \brief CaliWriter implementation

#pragma once

#include "caliper/common/Entry.h"

#include <memory>
#include <vector>

namespace cali
{

class CaliperMetadataAccessInterface;
class Node;
class OutputStream;
    
class CaliWriter
{
    struct CaliWriterImpl;
    std::shared_ptr<CaliWriterImpl> mP;

public:

    CaliWriter()
        { }
    
    CaliWriter(OutputStream& os);

    ~CaliWriter();

    size_t num_written() const;
    
    void write_snapshot(const CaliperMetadataAccessInterface&,
                        const std::vector<Entry>&);

    void write_globals(const CaliperMetadataAccessInterface&,
                       const std::vector<Entry>&);
};

}

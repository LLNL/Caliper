// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file  CaliReader.h
/// \brief CaliReader class definition

#pragma once

#include "RecordProcessor.h"

#include <memory>
#include <string>

namespace cali
{

class CaliperMetadataDB;

class CaliReader
{
    struct CaliReaderImpl;
    std::unique_ptr<CaliReaderImpl> mP;

public:

    CaliReader(const std::string& filename);

    ~CaliReader();

    bool read(CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc);
};

} // namespace cali

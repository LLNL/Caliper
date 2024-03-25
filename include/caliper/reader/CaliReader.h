// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file  CaliReader.h
/// \brief CaliReader class definition

#pragma once

#include "RecordProcessor.h"

#include <iostream>
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

    CaliReader();
    ~CaliReader();

    bool error() const;
    std::string error_msg() const;

    void read(std::istream& is, CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc);
    void read(const std::string& filename, CaliperMetadataDB& db, NodeProcessFn node_proc, SnapshotProcessFn snap_proc);
};

} // namespace cali

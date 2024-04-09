// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file  SnapshotTextFormatter.h
/// \brief Interface for Snapshot text formatter

#ifndef CALI_SNAPSHOT_TEXT_FORMATTER_H
#define CALI_SNAPSHOT_TEXT_FORMATTER_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace cali
{

class CaliperMetadataAccessInterface;
class Entry;

class SnapshotTextFormatter
{
    struct SnapshotTextFormatterImpl;
    std::unique_ptr<SnapshotTextFormatterImpl> mP;

public:

    SnapshotTextFormatter(const std::string& format_str = "");

    ~SnapshotTextFormatter();

    void
    reset(const std::string& format_str);

    std::ostream&
    print(std::ostream&, const CaliperMetadataAccessInterface&, const std::vector<Entry>&);
};

}

#endif

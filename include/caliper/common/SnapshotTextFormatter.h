/// \file  SnapshotTextFormatter.h
/// \brief Interface for Snapshot text formatter

#ifndef CALI_SNAPSHOT_TEXT_FORMATTER_H
#define CALI_SNAPSHOT_TEXT_FORMATTER_H

#include "Entry.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace cali
{

class CaliperMetadataAccessInterface;

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

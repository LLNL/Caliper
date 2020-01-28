// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file TreeFormatter.h
/// \brief TreeFormatter output formatter

#pragma once

#include "Formatter.h"
#include "RecordProcessor.h"

#include <iostream>
#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;
struct QuerySpec;

/// \brief Print a set of snapshot records in a tree 
/// \ingroup ReaderAPI
    
class TreeFormatter : public Formatter
{
    struct TreeFormatterImpl;
    std::shared_ptr<TreeFormatterImpl> mP;

public:

    TreeFormatter(const QuerySpec& spec);

    ~TreeFormatter();

    void process_record(CaliperMetadataAccessInterface&, const EntryList&);

    void flush(CaliperMetadataAccessInterface&, std::ostream& os);
};

} // namespace cali


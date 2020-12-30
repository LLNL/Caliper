// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file RecordSelector.h
/// \brief Defines RecordSelector

#ifndef CALI_RECORDSELECTOR_H
#define CALI_RECORDSELECTOR_H

#include "QuerySpec.h"
#include "RecordProcessor.h"

#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;

/// \brief Filter for snapshot records
/// \ingroup ReaderAPI
    
class RecordSelector 
{
    struct RecordSelectorImpl;
    std::shared_ptr<RecordSelectorImpl> mP;

public:

    RecordSelector(const std::string& filter_string);
    RecordSelector(const QuerySpec& spec);
    RecordSelector(const QuerySpec::Condition& cond);
    
    ~RecordSelector();

    bool pass(const CaliperMetadataAccessInterface&, const EntryList&);

    void operator()(CaliperMetadataAccessInterface&, const EntryList&, SnapshotProcessFn) const;

    static std::vector<QuerySpec::Condition> parse(const std::string&);
};

} // namespace cali

#endif

// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

///@file Expand.h
/// Expand output formatter declarations

#ifndef CALI_EXPAND_H
#define CALI_EXPAND_H

#include "Formatter.h"
#include "RecordProcessor.h"

#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;
class OutputStream;
struct QuerySpec;

/// \brief Prints expanded snapshot records in CSV form
/// \ingroup ReaderAPI
    
class Expand : public Formatter
{
    struct ExpandImpl;
    std::shared_ptr<ExpandImpl> mP;

public:

    Expand(OutputStream& os, const std::string& filter_string);
    Expand(OutputStream& os, const QuerySpec& spec);

    ~Expand();

    void operator()(CaliperMetadataAccessInterface&, const EntryList&) const;

    void process_record(CaliperMetadataAccessInterface&, const EntryList&);
};

} // namespace cali

#endif

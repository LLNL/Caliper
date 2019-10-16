// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

///@file Format.h
/// Format output formatter declarations

#ifndef CALI_FORMAT_H
#define CALI_FORMAT_H

#include "Formatter.h"
#include "RecordProcessor.h"

#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;
class OutputStream;
struct QuerySpec;

/// \brief Prints snapshot records using a user-defined format string
/// \ingroup ReaderAPI
    
class UserFormatter : public Formatter
{
    struct FormatImpl;
    std::shared_ptr<FormatImpl> mP;

public:

    UserFormatter(OutputStream& os, const QuerySpec& spec);

    ~UserFormatter();

    void process_record(CaliperMetadataAccessInterface&, const EntryList&);
};

} // namespace cali

#endif

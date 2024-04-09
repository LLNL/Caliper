// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Json.h
/// Json output formatter

#pragma once

#include "Formatter.h"

#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;
class OutputStream;

struct QuerySpec;

/// \brief Prints snapshot records as sparse JSON
/// \ingroup ReaderAPI
class JsonFormatter : public Formatter
{
    struct JsonFormatterImpl;
    std::shared_ptr<JsonFormatterImpl> mP;

public:

    JsonFormatter(OutputStream& os, const QuerySpec& spec);

    ~JsonFormatter();

    void process_record(CaliperMetadataAccessInterface&, const EntryList&);

    void flush(CaliperMetadataAccessInterface&, std::ostream& os);
};

} // namespace cali

// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Preprocessor.h
/// \brief Defines the Preprocessor class

#ifndef CALI_PREPROCESSOR_H
#define CALI_PREPROCESSOR_H

#include "QuerySpec.h"
#include "RecordProcessor.h"

#include <iostream>
#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;

/// \brief Preprocess %Caliper records. Handles the CalQL \a LET clause.
/// \ingroup ReaderAPI

class Preprocessor
{
    struct PreprocessorImpl;
    std::shared_ptr<PreprocessorImpl> mP;

public:

    Preprocessor(const QuerySpec& spec);

    ~Preprocessor();

    EntryList process(CaliperMetadataAccessInterface&, const EntryList&);

    void operator()(CaliperMetadataAccessInterface& db, const EntryList& rec, SnapshotProcessFn push) {
        push(db, process(db, rec));
    }

    static const QuerySpec::FunctionSignature*
    preprocess_defs();
};

} // namespace cali

#endif

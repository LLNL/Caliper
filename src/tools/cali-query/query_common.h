// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file query_common.h
/// \brief Common functionality for cali-query and mpi-caliquery

#pragma once

#include "caliper/reader/QuerySpec.h"
#include "caliper/reader/RecordProcessor.h"

namespace util
{

class Args;

}

namespace cali
{

class CaliperMetadataAccessInterface;
class ConfigManager;

/// \brief Create QuerySpec from command-line arguments
class QueryArgsParser {
    bool        m_error;
    std::string m_error_msg;
    QuerySpec   m_spec;

public:

    QueryArgsParser()
        : m_error(true),
          m_error_msg("query not read")
    { }

    /// \brief Get query spec from cali-query command-line arguments.
    /// \return Returns \a true if successful, `false` in case of error.
    bool parse_args(const util::Args& args);

    bool        error() const     { return m_error;     }
    std::string error_msg() const { return m_error_msg; }

    QuerySpec   spec() const      { return m_spec;      }
};

/// \class SnapshotFilterStep
/// \brief Basically the chain link in the processing chain.
///    Passes result of \a m_filter_fn to \a m_push_fn.
struct SnapshotFilterStep {
    SnapshotFilterFn  m_filter_fn;  ///< This processing step
    SnapshotProcessFn m_push_fn;    ///< Next processing step

    SnapshotFilterStep(SnapshotFilterFn filter_fn, SnapshotProcessFn push_fn)
        : m_filter_fn { filter_fn }, m_push_fn { push_fn }
    { }

    void operator ()(CaliperMetadataAccessInterface& db, const EntryList& list) {
        m_filter_fn(db, list, m_push_fn);
    }
};

/// \brief Process --help for cali-query and mpi-caliquery
void print_caliquery_help(const util::Args& args, const char* usage, const ConfigManager&);

}

// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file SnapshotTableFormatter.h
/// \brief Defines SnapshotTableFormatter class

#ifndef CALI_SNAPSHOTTABLEFORMATTER_H
#define CALI_SNAPSHOTTABLEFORMATTER_H

#include "caliper/common/Entry.h"

#include <vector>

namespace cali
{

class CaliperMetadataAccessInterface;

std::ostream&
format_record_as_table(CaliperMetadataAccessInterface& db, const std::vector<Entry>&, std::ostream& os);

} // namespace cali

#endif

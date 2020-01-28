// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file AttributeExtract.h
/// \brief A class to extract attributes as snapshots

#pragma once

#include "caliper/reader/RecordProcessor.h"

#include <memory>

namespace cali
{

/// This class is a node processor that converts attribute nodes into a snapshot record,
/// and forwards them to a snapshot processor
class AttributeExtract
{
    struct AttributeExtractImpl;
    std::shared_ptr<AttributeExtractImpl> mP;

public:

    AttributeExtract(SnapshotProcessFn snap_fn);

    ~AttributeExtract();

    void operator()(CaliperMetadataAccessInterface&, const Node*);
};

}

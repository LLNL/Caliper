// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file RecordProcessor.h
/// \brief Various type definitions for the reader API
/// \ingroup ReaderAPI

#pragma once

#include "../common/Entry.h"

#include <functional>
#include <vector>

namespace cali
{
    class CaliperMetadataAccessInterface;

    typedef std::vector<Entry> 
        EntryList;

    typedef std::function<void(CaliperMetadataAccessInterface& db,const Node* node)> 
        NodeProcessFn;
    typedef std::function<void(CaliperMetadataAccessInterface& db,const Node* node,NodeProcessFn)> 
        NodeFilterFn;

    typedef std::function<void(CaliperMetadataAccessInterface& db,const EntryList& list)> 
        SnapshotProcessFn;
    typedef std::function<void(CaliperMetadataAccessInterface& db,const EntryList& list,SnapshotProcessFn)> 
        SnapshotFilterFn;

} // namespace cali

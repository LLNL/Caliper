///@file RecordSelector.h
///RecordProcessor declarations

#ifndef CALI_RECORDSELECTOR_H
#define CALI_RECORDSELECTOR_H

#include "RecordProcessor.h"

#include <memory>

namespace cali
{

class CaliperMetadataDB;

class RecordSelector 
{
    struct RecordSelectorImpl;
    std::shared_ptr<RecordSelectorImpl> mP;

public:

    RecordSelector(const std::string& filter_string);

    ~RecordSelector();

    void operator()(CaliperMetadataDB&, const RecordMap&, RecordProcessFn) const;
};

} // namespace cali

#endif

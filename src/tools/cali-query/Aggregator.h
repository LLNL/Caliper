///@file Aggregator.h
/// Aggregator declarations

#ifndef CALI_AGGREGATOR_H
#define CALI_AGGREGATOR_H

#include "RecordProcessor.h"

#include <iostream>
#include <memory>

namespace cali
{

class CaliperMetadataDB;

class Aggregator 
{
    struct AggregatorImpl;
    std::shared_ptr<AggregatorImpl> mP;

public:

    Aggregator(const std::string& aggr_config);

    ~Aggregator();

    void operator()(CaliperMetadataDB&, const RecordMap&, RecordProcessFn push);

    void flush(CaliperMetadataDB&, RecordProcessFn push);
};

} // namespace cali

#endif

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

    Aggregator(const std::string& aggr_config, RecordProcessFn push_fn);

    ~Aggregator();

    void operator()(CaliperMetadataDB&, const RecordMap&);

    void flush(CaliperMetadataDB&);
};

} // namespace cali

#endif

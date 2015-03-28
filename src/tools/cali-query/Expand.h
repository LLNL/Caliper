///@file Expand.h
/// Expand output formatter declarations

#ifndef CALI_EXPAND_H
#define CALI_EXPAND_H

#include <iostream>
#include <memory>

namespace cali
{

class CaliperMetadataDB;

class Expand 
{
    struct ExpandImpl;
    std::shared_ptr<ExpandImpl> mP;

public:

    Expand(std::ostream& os, const std::string& filter_string);

    ~Expand();

    void operator()(CaliperMetadataDB&, const RecordMap&) const;
};

} // namespace cali

#endif

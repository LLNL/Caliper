///@file Expand.h
///RecordProcessor declarations

#ifndef CALI_FIELDSELECTOR_H
#define CALI_FIELDSELECTOR_H

#include "RecordProcessor.h"

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

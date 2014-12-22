/// @file ContextRecord.h
/// ContextRecord class definition

#ifndef CALI_CONTEXTRECORD_H
#define CALI_CONTEXTRECORD_H

#include "cali_types.h"

#include "Attribute.h"
#include "RecordMap.h"

#include <functional>
#include <vector>

namespace cali
{

class Node;

class ContextRecord
{

public:

    static 
    std::vector<RecordMap> 
    unpack(std::function<cali::Attribute  (cali_id_t)> get_attr, 
           std::function<const cali::Node*(cali_id_t)> get_node,
           const uint64_t buf[], size_t size);
};

}

#endif

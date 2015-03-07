/// @file ContextRecord.h
/// ContextRecord class definition

#ifndef CALI_CONTEXTRECORD_H
#define CALI_CONTEXTRECORD_H

#include "cali_types.h"

#include "Record.h"
#include "RecordMap.h"

#include <functional>


namespace cali
{

class Node;

class ContextRecord
{
    static const RecordDescriptor s_record;

public:

    static 
    RecordMap
    unpack(const RecordMap& rec, std::function<const cali::Node*(cali_id_t)> get_node);

    static const RecordDescriptor& record_descriptor() { return s_record; }
};

}

#endif

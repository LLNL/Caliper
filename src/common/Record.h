///@file Record.h
///Record interface declaration

#ifndef CALI_RECORD_H
#define CALI_RECORD_H

#include <iostream>

#include <functional>

namespace cali
{

class Variant;

// --- Record API

struct RecordDescriptor {
    uint32_t     id;
    const char*  name;
    unsigned     num_entries;
    const char** entries;
};

typedef std::function<void(const RecordDescriptor&,const int*,const Variant**)> WriteRecordFn;

}

#endif // CALI_RECORD_H

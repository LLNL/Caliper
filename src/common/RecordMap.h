///@file RecordMap.h
///RecordMap interface declaration

#ifndef CALI_RECORDMAP_H
#define CALI_RECORDMAP_H

#include "Variant.h"

#include <iostream>
#include <map>
#include <string>

namespace cali
{

// --- RecordMap API

typedef std::map< std::string, Variant > RecordMap;

}

std::ostream& operator << (std::ostream& os, const cali::RecordMap& r);

#endif // CALI_RECORDMAP_H

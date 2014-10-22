///@file Query.cpp
/// Query interface implementation

#include "RecordMap.h"

#include <iostream>


std::ostream& operator << (std::ostream& os, const cali::RecordMap& r)
{
    os << "{ ";

    for (const auto &p : r)
        os << p.first << '=' << p.second << ' ';

    os << '}';
    
    return os;
}

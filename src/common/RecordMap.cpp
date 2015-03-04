///@file Query.cpp
/// Query interface implementation

#include "RecordMap.h"

#include <iostream>


std::ostream& operator << (std::ostream& os, const cali::RecordMap& record)
{
    int count = 0;

    for (const auto &entry : record) {
        if (!entry.second.empty())
            os << (count++ ? "," : "") << entry.first;
        for (const auto &elem : entry.second)
            os << '=' << elem;
    }

    return os;
}

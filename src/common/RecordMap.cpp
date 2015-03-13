///@file Query.cpp
/// Query interface implementation

#include "RecordMap.h"

#include <iostream>

std::string cali::get_record_type(const cali::RecordMap& rec)
{
    auto rec_entry_it = rec.find("__rec");

    if (rec_entry_it != rec.end() && !rec_entry_it->second.empty())
        return rec_entry_it->second.front().to_string();

    return { };
}

std::ostream& cali::operator << (std::ostream& os, const cali::RecordMap& record)
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

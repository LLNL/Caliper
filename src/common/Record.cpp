///@file Record.cpp
///Record class definition

#include "Record.h"

using namespace cali;
using namespace std;

const Record::Key Record::record_type_key { 0x01, "__rec" };
const uint32_t    Record::first_user_id   { 0x100 };

Record::Record(unsigned n, const Key keys[], const Variant entries[])
{
    m_entries.reserve(n);
    m_data.reserve(3*n);

    for (unsigned i = 0; i < n; ++i) {
        m_entries.emplace_back(keys[i], 1);

        m_data.emplace_back(keys[i].id);
        m_data.emplace_back(1);
        m_data.emplace_back(entries[i]);
    }
}

std::ostream& operator << (std::ostream& os, const cali::Record& r)
{
    unsigned count = 0;

    for (const Variant& v : r.raw_data())
        os << (count++ > 0 ? "," : "") << v.to_string();

    if (count)
        os << std::endl;
}

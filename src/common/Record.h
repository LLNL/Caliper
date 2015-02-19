///@file Record.h
///Record interface declaration

#ifndef CALI_RECORD_H
#define CALI_RECORD_H

#include "Variant.h"

#include <iostream>
#include <vector>

namespace cali
{

// --- Record API


class Record 
{
public:

    struct Key {
        uint32_t    id;
        const char* name;
    };

    static const Key      record_type_key;
    static const uint32_t first_user_id;

    Record(unsigned n, const Key keys[], const cali::Variant entries[]);
//    Record(unsigned n, const Key keys[], const unsigned num_entries[], const Variant* entries[]);

    Record(const Record&) = default;
    Record(Record&&) = default;

    ~Record() { }

    Record& operator = (const Record&) = default;
    Record& operator = (Record&&) = default;

    const std::vector<cali::Variant>& raw_data() const { return m_data; }

private:

    struct Element {
        Key      key;
        unsigned num_entries;

        Element(const Key& k, unsigned n)
            : key(k), num_entries(n) { }
    };

    std::vector<Element>       m_entries;
    std::vector<cali::Variant> m_data;
};

}

std::ostream& operator << (std::ostream& os, const cali::Record& r);

#endif // CALI_RECORD_H

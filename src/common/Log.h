/// @file Log.h
/// Log class declaration

#ifndef CALI_LOG_H
#define CALI_LOG_H

#include <iostream>
#include <fstream>

namespace cali
{

class Log
{
    std::ofstream m_nullstream;
    unsigned      m_level;

    std::ostream& get_stream();

public:

    static unsigned verbosity();

    inline std::ostream& stream() {
        if (verbosity() < m_level)
            return m_nullstream;

        return get_stream();
    }

    Log(unsigned level = 1)
        : m_level { level }
        { }
};

} // namespace cali

#endif // CALI_LOG_H

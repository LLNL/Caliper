// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Log.h
/// Log class declaration

#ifndef CALI_LOG_H
#define CALI_LOG_H

#include <iostream>
#include <fstream>

namespace cali
{

/// \brief A logging stream
class Log
{
    std::ofstream m_nullstream;
    unsigned      m_level;

    std::ostream& get_stream();

public:

    static unsigned verbosity();
    static void set_verbosity(unsigned v);
    static void add_prefix(const std::string& prefix);

    inline std::ostream& stream() {
        if (verbosity() < m_level)
            return m_nullstream;

        return get_stream();
    }

    /// \brief Print error message for a C/POSIX errno on the log stream
    ///
    /// Prints an error message for an \a errno value set by a POSIX call.
    /// Does not append a newline, users should add a line break explicitly if
    /// needed. Example:
    ///
    /// \code
    ///    const char* filename = "/usr/bin/ls";
    ///    if (open(filename, O_RDWD) == -1) {
    ///        Log(0).perror(errno, "open: ") << ": " << filename << std::endl;
    ///        // possible output: "open: Permission denied: /usr/bin/ls"
    ///    }
    /// \endcode
    ///
    /// \param \errnum The errno value
    /// \param \msg Optional prefix message
    /// \return The log stream's \a std::ostream object
    std::ostream& perror(int errnum, const char* msg = "");

    Log(unsigned level = 1)
        : m_level { level }
        { }
};

} // namespace cali

#endif // CALI_LOG_H

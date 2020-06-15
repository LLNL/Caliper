// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// @file Log.cpp
/// Log class implementation

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <cstring>
#include <memory>
#include <map>

using namespace cali;
using namespace std;


struct LogImpl
{
    // --- data

    static const char*            s_prefix;
    static const ConfigSet::Entry s_configdata[];

    static LogImpl*               s_instance;

    enum class Stream { StdOut, StdErr, None, File };

    ConfigSet     m_config;

    Stream        m_stream;
    std::ofstream m_ofstream;
    int           m_verbosity;

    std::string   m_prefix;

    // --- helpers

    void init_stream(const std::string& name) {
        const map<string, Stream> strmap {
            { "none",   Stream::None   },
            { "stdout", Stream::StdOut },
            { "stderr", Stream::StdErr } };

        auto it = strmap.find(name);

        if (it == strmap.end()) {
            m_stream = Stream::File;

            m_ofstream.open(name);

            if (!m_ofstream && m_verbosity > 0)
                cerr << s_prefix << "Could not open log file " << name << endl;
        } else
            m_stream = it->second;
    }

    // --- interface

    LogImpl()
        : m_prefix { s_prefix }
    {
        ConfigSet config =
            RuntimeConfig::get_default_config().init("log", s_configdata);

        m_verbosity = config.get("verbosity").to_int();
        init_stream(config.get("logfile").to_string());
    }

    ostream& get_stream() {
        switch (m_stream) {
        case Stream::StdOut:
            return std::cout;
        case Stream::StdErr:
            return std::cerr;
        default:
            return m_ofstream;
        }
    }
};

const char*            LogImpl::s_prefix = "== CALIPER: ";
const ConfigSet::Entry LogImpl::s_configdata[] = {
    // key, type, value, short description, long description
    { "verbosity", CALI_TYPE_UINT,   "0",
      "Verbosity level",
      "Verbosity level.\n"
      "  0: no output\n"
      "  1: basic informational runtime output\n"
      "  2: debug output"
    },
    { "logfile",   CALI_TYPE_STRING, "stderr",
      "Log file name",
      "Log file name or output stream. Either one of\n"
      "   stdout: Standard output stream,\n"
      "   stderr: Standard error stream,\n"
      "   none:   No output,\n"
      " or a log file name."
    },
    ConfigSet::Terminator
};

LogImpl* LogImpl::s_instance = nullptr;


//
// --- Log public interface
//

ostream&
Log::get_stream()
{
    return LogImpl::s_instance->get_stream() << LogImpl::s_instance->m_prefix;
}

ostream&
Log::perror(int errnum, const char* msg)
{
    if (verbosity() < m_level)
        return m_nullstream;

#ifdef _GLIBCXX_HAVE_STRERROR_R
    char buf[120];
    return get_stream() << msg << strerror_r(errnum, buf, sizeof(buf));
#else
    return get_stream() << msg << strerror(errnum);
#endif
}

int
Log::verbosity()
{
    if (LogImpl::s_instance == nullptr)
        return -1;

    return LogImpl::s_instance->m_verbosity;
}

void
Log::set_verbosity(int v)
{
    LogImpl::s_instance->m_verbosity = v;
}

void
Log::add_prefix(const std::string& prefix)
{
    LogImpl::s_instance->m_prefix += prefix;
}

void
Log::init()
{
    LogImpl::s_instance = new LogImpl;
}

void
Log::fini()
{
    delete LogImpl::s_instance;
    LogImpl::s_instance = nullptr;
}

bool
Log::is_initialized()
{
    return LogImpl::s_instance != nullptr;
}

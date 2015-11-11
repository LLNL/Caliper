// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// @file Log.cpp
/// Log class implementation

#include "Log.h"
#include "RuntimeConfig.h"

#include <memory>
#include <map>

using namespace cali;
using namespace std;


struct LogImpl
{
    // --- data

    static unique_ptr<LogImpl>    s_instance;
    static const char*            s_prefix;

    static const ConfigSet::Entry s_configdata[];

    enum class Stream { StdOut, StdErr, None, File };

    ConfigSet     m_config;

    Stream        m_stream;
    std::ofstream m_ofstream;
    unsigned      m_verbosity;

    std::string   m_prefix;

    // --- helpers

    void init_stream() {
        string name = m_config.get("logfile").to_string();

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
        : m_config { RuntimeConfig::init("log", s_configdata) },
          m_prefix { s_prefix }
    {
        m_verbosity = m_config.get("verbosity").to_uint();
        init_stream();
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

    static LogImpl* instance() {
        if (!s_instance)
            s_instance.reset(new LogImpl);

        return s_instance.get();
    }
};

unique_ptr<LogImpl>    LogImpl::s_instance { nullptr } ;
const char*            LogImpl::s_prefix   { "== CALIPER: " };

const ConfigSet::Entry LogImpl::s_configdata[] = {
    // key, type, value, short description, long description
    { "verbosity", CALI_TYPE_UINT,   "1",
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


//
// --- Log public interface
//

ostream& 
Log::get_stream()
{
    return (LogImpl::instance()->get_stream() << LogImpl::instance()->m_prefix);
}

unsigned 
Log::verbosity()
{
    return LogImpl::instance()->m_verbosity;
}

void 
Log::set_verbosity(unsigned v)
{
    LogImpl::instance()->m_verbosity = v;
}

void 
Log::add_prefix(const std::string& prefix)
{
    LogImpl::instance()->m_prefix += prefix;
}

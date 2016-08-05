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

/// @file Recorder.cpp
/// @brief Caliper event recorder

#include "../CaliperService.h"

#include <Caliper.h>

#include <csv/CsvSpec.h>

#include <ContextRecord.h>
#include <Log.h>
#include <RuntimeConfig.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <fstream>
#include <random>
#include <string>

using namespace cali;
using namespace std;

namespace 
{

class Recorder
{    
    static unique_ptr<Recorder>   s_instance;
    static const ConfigSet::Entry s_configdata[];

    enum class Stream { None, File, StdErr, StdOut };

    ConfigSet  m_config;

    bool       m_stream_initialized;
    
    Stream     m_stream;
    ofstream   m_ofstream;

    std::mutex m_lock;

    int        m_reccount;
    
    // --- helpers

    string random_string(string::size_type len) {
        static std::mt19937 rgen(chrono::system_clock::now().time_since_epoch().count());
        
        static const char characters[] = "0123456789"
            "abcdefghiyklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXZY";

        std::uniform_int_distribution<> random(0, sizeof(characters)-2);

        string s(len, '-');

        generate_n(s.begin(), len, [&](){ return characters[random(rgen)]; });

        return s;
    }

    string create_filename() {
        char   timestring[16];
        time_t tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
        strftime(timestring, sizeof(timestring), "%y%m%d-%H%M%S", localtime(&tm));

        int  pid = static_cast<int>(getpid());

        return string(timestring) + "_" + std::to_string(pid) + "_" + random_string(12) + ".cali";
    }

    void init_recorder() {
        string filename = m_config.get("filename").to_string();

        const map<string, Stream> strmap { 
            { "none",   Stream::None   },
            { "stdout", Stream::StdOut },
            { "stderr", Stream::StdErr } };

        auto it = strmap.find(filename);

        if (it == strmap.end())
            m_stream = Stream::File;
        else {
            m_stream = it->second;
            m_stream_initialized = true;
        }
    }

    void init_stream() {
        if (m_stream == Stream::File) {
            string filename = m_config.get("filename").to_string();
            
            if (filename.empty())
                filename = create_filename();

            string dirname = m_config.get("directory").to_string();
            struct stat s = { 0 };

            if (!dirname.empty()) {
                if (stat(dirname.c_str(), &s) < 0)
                    Log(0).stream() << "Warning: Could not open Caliper output directory " << dirname << endl;
                else
                    dirname += '/';
            }

            m_ofstream.open(dirname + filename);

            if (!m_ofstream) {
                Log(0).stream() << "Could not open recording file " << filename << endl;
                m_stream = Stream::None;
            } else
                m_stream = Stream::File;
        }

        m_stream_initialized = true;
    }
    
    std::ostream& get_stream() {
        if (!m_stream_initialized)
            init_stream();
        
        switch (m_stream) {
        case Stream::StdOut:
            return std::cout;
        case Stream::StdErr:
            return std::cerr;
        default:
            return m_ofstream;
        }
    }

    static void write_record_cb(const RecordDescriptor& rec, const int* count, const Variant** data) {
        if (!s_instance){
            std::cout<<"CANNOT WRITE: NOT YET INITIALIZED"<<std::endl;
            return;
        }

        std::lock_guard<std::mutex> g(s_instance->m_lock);
        
        CsvSpec::write_record(s_instance->get_stream(), rec, count, data);
        ++s_instance->m_reccount;
    }

    static void finish_cb(Caliper* c) {
        if (s_instance)
            Log(1).stream() << "Wrote " << s_instance->m_reccount << " records." << endl;
    }
    
    void register_callbacks(Caliper* c) {
        c->events().write_record.connect(write_record_cb);
        c->events().finish_evt.connect(finish_cb);
    }

    Recorder(Caliper* c)
        : m_config   { RuntimeConfig::init("recorder", s_configdata) },
          m_stream_initialized { false },
          m_reccount { 0 }
    { 
        init_recorder();

        if (m_stream != Stream::None) {
            register_callbacks(c);

            Log(1).stream() << "Registered recorder service" << endl;
        }
    }

public:

    ~Recorder() 
        { }

    static void create(Caliper* c) {
        s_instance.reset(new Recorder(c));
    }
};

unique_ptr<Recorder>   Recorder::s_instance       { nullptr };

const ConfigSet::Entry Recorder::s_configdata[] = {
    { "filename", CALI_TYPE_STRING, "",
      "File name for event record stream. Auto-generated by default.",
      "File name for event record stream. Either one of\n"
      "   stdout: Standard output stream,\n"
      "   stderr: Standard error stream,\n"
      "   none:   No output,\n"
      " or a file name. By default, a filename is auto-generated.\n"
    },
    { "directory", CALI_TYPE_STRING, "",
      "Name of Caliper output directory (default: current working directory)",
      "Name of Caliper output directory (default: current working directory)"
    },
    ConfigSet::Terminator
};

} // namespace

namespace cali
{
    CaliperService recorder_service { "recorder", &(::Recorder::create) };
}

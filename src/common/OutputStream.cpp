// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
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

#include "caliper/common/OutputStream.h"

#include "caliper/common/Log.h"
#include "caliper/common/SnapshotTextFormatter.h"

#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>

using namespace cali;

struct OutputStream::OutputStreamImpl
{
    StreamType    type;

    bool          is_initialized;
    std::mutex    init_mutex;
    
    std::string   filename;
    std::ofstream fs;

    std::ostream* user_os;

    void init() {        
        if (is_initialized)
            return;

        std::lock_guard<std::mutex>
            g(init_mutex);
        
        is_initialized = true;

        if (type == StreamType::File) {
            fs.open(filename);

            if (!fs.is_open()) {
                type = StreamType::None;
                
                Log(0).stream() << "Could not open output stream " << filename << std::endl;
            }
        }
    }

    std::ostream* stream() {
        init();
    
        switch (type) {
        case None:
            return &fs;
        case StdOut:
            return &std::cout;
        case StdErr:
            return &std::cerr;
        case File:
            return &fs;
        case User:
            return user_os;
        }
    }

    void reset() {
        fs.close();
        filename.clear();
        user_os = nullptr;
        type = StreamType::None;
        is_initialized = false;
    }
    
    OutputStreamImpl()
        : type(StreamType::None), is_initialized(false), user_os(nullptr)
    { }

    OutputStreamImpl(const char* name)
        : type(StreamType::None), is_initialized(false), filename(name), user_os(nullptr)
    { }
};

OutputStream::OutputStream()
    : mP(new OutputStreamImpl)
{ }

OutputStream::~OutputStream()
{
    mP.reset();
}

OutputStream::operator bool() const
{
    return mP->is_initialized;
}

OutputStream::StreamType
OutputStream::type() const
{
    return mP->type;
}

std::ostream*
OutputStream::stream()
{
    return mP->stream();
}

void
OutputStream::set_stream(StreamType type)
{
    mP->reset();
    mP->type = type;
}

void
OutputStream::set_stream(std::ostream* os)
{
    mP->reset();
    
    mP->type    = StreamType::User;
    mP->user_os = os;
}

void
OutputStream::set_filename(const char* filename)
{
    mP->reset();

    mP->filename = filename;
    mP->type     = StreamType::File;
}

void
OutputStream::set_filename(const char* formatstr, const CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec)
{
    mP->reset();
    
    if      (strcmp(formatstr, "stdout") == 0)
        mP->type = StreamType::StdOut;
    else if (strcmp(formatstr, "stderr") == 0)
        mP->type = StreamType::StdErr;
    else {
        SnapshotTextFormatter formatter(formatstr);
        std::ostringstream    fnamestr;

        formatter.print(fnamestr, db, rec);
        
        mP->filename = fnamestr.str();
        mP->type     = StreamType::File;
    }
}

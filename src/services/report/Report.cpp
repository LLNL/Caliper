// Copyright (c) 2016, Lawrence Livermore National Security, LLC.  
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

/// \file  Report.cpp
/// \brief Generates text reports from Caliper snapshots on flush() events 


#include "../CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/QueryProcessor.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/RuntimeConfig.h"

#include <iostream>

using namespace cali;

namespace
{

    class Report {
        static std::unique_ptr<Report> s_instance;
        static const ConfigSet::Entry  s_configdata[];

        QueryProcessor m_query;

        void process_snapshot(Caliper* c, const SnapshotRecord* snapshot) {
            m_query.process_record(*c, snapshot->to_entrylist());
        }

        void flush(Caliper* c, const SnapshotRecord*) {
            m_query.flush(*c);
        }

        Report(const QueryProcessor& query) 
            : m_query(query)
            { }

        //
        // --- callback functions
        //

        static void pre_flush_cb(Caliper* c, const SnapshotRecord* flush_info) {
            ConfigSet    config(RuntimeConfig::init("report", s_configdata));

            CalQLParser  parser(config.get("config").to_string().c_str());

            if (parser.error()) {
                Log(0).stream() << "report: config parse error: " << parser.error_msg() << std::endl;
                return;
            }

            QuerySpec    spec(parser.spec());

            // set format default to table if it hasn't been set in the query config
            if (spec.format.opt == QuerySpec::FormatSpec::Default)
                spec.format = CalQLParser("format table").spec().format;
                
            OutputStream stream;

            stream.set_stream(OutputStream::StdOut);

            std::string filename = config.get("filename").to_string();

            if (!filename.empty())
                stream.set_filename(filename.c_str(), *c, flush_info->to_entrylist());
            
            s_instance.reset(new Report(QueryProcessor(spec, stream)));
        }

        static void flush_snapshot_cb(Caliper* c, const SnapshotRecord*, const SnapshotRecord* snapshot) {
            if (!s_instance)
                return;

            s_instance->process_snapshot(c, snapshot);
        }

        static void flush_finish_cb(Caliper* c, const SnapshotRecord* flush_info) {
            if (!s_instance)
                return;

            s_instance->flush(c, flush_info);
        }

    public:

        ~Report()
            { }

        static void create(Caliper* c) {
            c->events().pre_flush_evt.connect(pre_flush_cb);
            c->events().write_snapshot.connect(flush_snapshot_cb);
            c->events().post_write_evt.connect(flush_finish_cb);

            Log(1).stream() << "Registered report service" << std::endl;
        }
    };

    std::unique_ptr<Report> Report::s_instance { nullptr };
    
    const ConfigSet::Entry  Report::s_configdata[] = {
        { "filename", CALI_TYPE_STRING, "stdout",
          "File name for report stream. Default: stdout.",
          "File name for report stream. Either one of\n"
          "   stdout: Standard output stream,\n"
          "   stderr: Standard error stream,\n"
          " or a file name.\n"
        },
        { "config", CALI_TYPE_STRING, "",
          "Report configuration/query specification in CalQL",
          "Report configuration/query specification in CalQL"
        },
        ConfigSet::Terminator
    };

} // namespace 

namespace cali
{
    CaliperService report_service { "report", ::Report::create };
}


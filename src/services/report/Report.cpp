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

#include "caliper/reader/Format.h"
#include "caliper/reader/RecordSelector.h"
#include "caliper/reader/Table.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/SnapshotTextFormatter.h"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace cali;

namespace
{

    class Report {
        static std::unique_ptr<Report> s_instance;
        static const ConfigSet::Entry  s_configdata[];

        ConfigSet      m_config;

        Table          m_table_writer;
        RecordSelector m_selector;

        std::vector<Entry> make_entrylist(Caliper* c, const SnapshotRecord* snapshot) {
            std::vector<Entry> list;

            if (!snapshot)
                return list;

            SnapshotRecord::Data  data(snapshot->data());
            SnapshotRecord::Sizes size(snapshot->size());

            for (size_t n = 0; n < size.n_nodes; ++n)
                list.push_back(Entry(data.node_entries[n]));
            for (size_t n = 0; n < size.n_immediate; ++n)
                list.push_back(Entry(c->get_attribute(data.immediate_attr[n]), data.immediate_data[n]));

            return list;
        }

        void process_snapshot(Caliper* c, const SnapshotRecord* snapshot) {
            SnapshotProcessFn fn(m_table_writer);

            m_selector(*c, snapshot->to_entrylist(), fn);
        }

        void flush(Caliper* c, const SnapshotRecord* flush_info) {
            std::string filename = m_config.get("filename").to_string();

            if (filename == "stdout")
                m_table_writer.flush(*c, std::cout);
            else if (filename == "stderr")
                m_table_writer.flush(*c, std::cerr);
            else {
                SnapshotTextFormatter formatter(filename);
                std::ostringstream    fnamestr;

                formatter.print(fnamestr, c, flush_info->to_entrylist());

                std::ofstream fs(fnamestr.str());

                if (!fs) {
                    Log(0).stream() << "Report: could not open output stream " << fnamestr.str() << std::endl;
                    return;
                }

                m_table_writer.flush(*c, fs);
            }
        }

        Report() 
            : m_config( RuntimeConfig::init("report", s_configdata) ),
              m_table_writer(m_config.get("attributes").to_string(),
                             m_config.get("sort_by").to_string()),
              m_selector(m_config.get("filter").to_string())  
            { }

        //
        // --- callback functions
        // 

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
            s_instance.reset(new Report);

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
        { "attributes", CALI_TYPE_STRING, "",
          "List of attributes (columns) to print.",
          "List of attributes (columns) to print. "
          "Default: empty (print all user-defined attributes).",
        },
        { "filter", CALI_TYPE_STRING, "",
          "Filter snapshots (rows) to print.",
          "Filter snapshots (rows) to print. Default: empty (print all).",
        },
        { "sort_by", CALI_TYPE_STRING, "",
          "List of attributes to sort by.",
          "List of attributes to sort by. Default: empty (undefined order)"
        },
        ConfigSet::Terminator
    };

} // namespace 

namespace cali
{
    CaliperService report_service { "report", ::Report::create };
}


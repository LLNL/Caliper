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

#include <Caliper.h>
#include <SnapshotRecord.h>

#include <Format.h>
#include <RecordSelector.h>
#include <Table.h>

#include <Log.h>
#include <Node.h>
#include <RuntimeConfig.h>

#include <iostream>
#include <fstream>

using namespace cali;

namespace
{

    class Report {
        static std::unique_ptr<Report> s_instance;
        static const ConfigSet::Entry  s_configdata[];

        ConfigSet      m_config;

        Table          m_table_writer;
        RecordSelector m_selector;

        void process_snapshot(Caliper* c, const SnapshotRecord* snapshot) {
            EntryList list;

            SnapshotRecord::Data  data(snapshot->data());
            SnapshotRecord::Sizes size(snapshot->size());

            for (size_t n = 0; n < size.n_nodes; ++n)
                list.push_back(Entry(data.node_entries[n]));
            for (size_t n = 0; n < size.n_immediate; ++n)
                list.push_back(Entry(c->get_attribute(data.immediate_attr[n]), data.immediate_data[n]));

            SnapshotProcessFn fn(m_table_writer);

             m_selector(*c, list, fn);
        }

        void flush(Caliper* c) {
            std::string filename = m_config.get("filename").to_string();

            if (filename == "stdout")
                m_table_writer.flush(*c, std::cout);
            else if (filename == "stderr")
                m_table_writer.flush(*c, std::cerr);
            else {
                std::ofstream fs(filename);

                if (!fs) {
                    Log(0).stream() << "report: could not open output stream " << filename << std::endl;
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

        static void flush_finish_cb(Caliper* c, const SnapshotRecord*) {
            if (!s_instance)
                return;

            s_instance->flush(c);
        }

    public:

        ~Report()
            { }

        static void create(Caliper* c) {
            s_instance.reset(new Report);

            c->events().flush_snapshot.connect(flush_snapshot_cb);
            c->events().flush_finish_evt.connect(flush_finish_cb);

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


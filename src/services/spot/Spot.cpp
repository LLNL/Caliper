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

// Spot.cpp
// Outputs Caliper performance results to Spot-parseable formats


#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/QueryProcessor.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/RuntimeConfig.h"

#include <iostream>
#include <sstream>
using namespace cali;

namespace
{

    class Spot {
        static std::unique_ptr<Spot> s_instance;
        static const ConfigSet::Entry  s_configdata[];

        std::vector<std::pair<std::stringstream*,QueryProcessor*>> m_queries;

        void process_snapshot(Caliper* c, const SnapshotRecord* snapshot) {
            for(auto entry : m_queries){
              m_query = entry.first;
              m_query.process_record(*c, snapshot->to_entrylist());
            }
        }

        void flush(Caliper* c, const SnapshotRecord*) {
            for(auto entry : m_queries){
              m_query = entry.first;
              m_query.flush(*c);
            }
        }

        Spot(const QueryProcessor& query) 
            : m_query(query)
            { }

        //
        // --- callback functions
        //
        static std::pair<std::stringstream*, QueryProcessor*> create_query_processor(std::string query){
           CalQLParser parser(query.c_str()); 
           std::stringstream* new_stream = new std::stringstream();
           if (parser.error()) {
               Log(0).stream() << "spot: config parse error: " << parser.error_msg() << std::endl;
               return;
           }
           QuerySpec    spec(parser.spec());
           OutputStream stream;
           stream.set_stream(*new_stream);
           QueryProcessor* new_proc = new QueryPorcessor(spec,stream);
           return std::make_pair(new_stream,new_proc);
        }
        std::string query_for_annotation(std::string grouping, std::string metric = "time.inclusive.duration"){
          group_comma_metric = " " +grouping+","+metric+" ";
          return "SELECT" + group_comma_metric + "WHERE" + group_comma_metric + "GROUP BY " +groupind + " FORMAT JSON()";
        }
        static void pre_write_cb(Caliper* c, const SnapshotRecord* flush_info) {
            ConfigSet    config(RuntimeConfig::init("spot", s_configdata));
            std::string  config_string = config.get("config").to_string().c_str();
            std::vector<std::string> logging_configurations;
            util::split(config_string,",",std::back_inserter(logging_configurations));
            for(const auto log_config : logging_configurations){
              std::vector<std::string> annotation_and_place;
              util::split(log_config,",",std::back_inserter(annotation_and_place));
              m_queries.emplace_back(create_query_processor(query_for_annotation(annotation_and_place[0])));
            }
            CalQLParser  parser(config.get("config").to_string().c_str());

            if (parser.error()) {
                Log(0).stream() << "spot: config parse error: " << parser.error_msg() << std::endl;
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
            
            s_instance.reset(new Spot(QueryProcessor(spec, stream)));
        }

        static void write_snapshot_cb(Caliper* c, const SnapshotRecord*, const SnapshotRecord* snapshot) {
            if (!s_instance)
                return;

            s_instance->process_snapshot(c, snapshot);
        }

        static void post_write_cb(Caliper* c, const SnapshotRecord* flush_info) {
            if (!s_instance)
                return;

            s_instance->flush(c, flush_info);
        }

    public:

        ~Spot()
            { }

        static void create(Caliper* c) {
            c->events().pre_write_evt.connect(pre_write_cb);
            c->events().write_snapshot.connect(write_snapshot_cb);
            c->events().post_write_evt.connect(post_write_cb);

            Log(1).stream() << "Registered Spot service" << std::endl;
        }
    };

    std::unique_ptr<Spot> Spot::s_instance { nullptr };
    
    const ConfigSet::Entry  Spot::s_configdata[] = {
        { "config", CALI_TYPE_STRING, "function:default.json",
          "Attribute:Filename pairs in which to dump Spot data",
          "Attribute:Filename pairs in which to dump Spot data\n"
          "Example: function:testname.json,physics_package:packages.json"
          "   stderr: Standard error stream,\n"
          " or a file name.\n"
        },
        { "recorded_time", CALI_TYPE_STRING, "",
          "Time to use for this version of the code",
          "Time to use for this version of the code"
        },
        { "code_version", CALI_TYPE_STRING, "",
          "Version number (or git hash) to represent this run of the code",
          "Version number (or git hash) to represent this run of the code"
        },
        ConfigSet::Terminator
    };

} // namespace 

namespace cali
{
    CaliperService spot_service { "spot", ::Spot::create };
}


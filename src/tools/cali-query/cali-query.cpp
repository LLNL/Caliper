// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// Modified by Aimee Sylvia
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

// A basic tool for Caliper metadata queries

#include "AttributeExtract.h"
#include "query_common.h"

#include "caliper/tools-util/Args.h"

#include "caliper/cali.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/RecordProcessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/common/ContextRecord.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/StringConverter.h"

#include "caliper/common/csv/CsvReader.h"
#include "caliper/common/csv/CsvWriter.h"

#include "caliper/common/util/split.hpp"

#include <atomic>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <thread>

using namespace cali;
using namespace std;
using namespace util;


namespace
{
    const char* usage = "cali-query [OPTION]... [FILE]..."
        "\n  Read, merge, and filter caliper streams";

    const Args::Table option_table[] = { 
        // name, longopt name, shortopt char, has argument, info, argument info
        { "select", "select", 's', true,  
          "Filter records by selected attributes: [-]attribute[(<|>|=)value][:...]", 
          "QUERY_STRING" 
        },
        { "aggregate", "aggregate", 'a', true,
          "Aggregate snapshots using the given aggregation operators: (sum(attribute)|count)[:...]",
          "AGGREGATION_OPS"
        },
        { "aggregate-key", "aggregate-key", 0, true,
          "List of attributes to aggregate over (collapses all other attributes): attribute[:...]",
          "ATTRIBUTES"
        },
        { "expand", "expand", 'e', false,  
          "Expand context records and print the selected attributes (default: all)", 
          nullptr 
        },
        { "attributes", "print-attributes", 0, true,  
          "Select attributes to print (or hide) in expanded output: [-]attribute[:...]", 
          "ATTRIBUTES" 
        },
        { "sort", "sort-by", 'S', true,  
          "Sort rows in table format: attribute[:...]", 
          "SORT_ATTRIBUTES" 
        },
	{ "format", "format", 'f', true,
          "Format output according to format string: %[<width+alignment(l|r|c)>]attr_name%...",
          "FORMAT_STRING"
        }, 
	{ "title",  "title",  'T', true,
          "Set the title row for formatted output",
          "STRING"
        }, 
        { "table", "table", 't', false,
          "Print attributes in human-readable table form",
          nullptr
        },
        { "tree" , "tree", 'T', false,
          "Print records in a tree based on the hierarchy of the selected path attributes",
          nullptr
        },
        { "path-attributes", "path-attributes", 0, true,
          "Select the path attributes for tree printers",
          "ATTRIBUTES"
        },
        { "json", "json", 'j', false,
          "Print given attributes in web-friendly json format",
          "ATTRIBUTES"
        },
        { "threads", "threads", 0, true,
          "Use this many threads (applicable only with multiple files)",
          "THREADS"
        },
        { "query", "query", 'q', true,
          "Execute a query in CalQL format",
          "QUERY STRING"
        },
        { "profile", "profile", 'p', false,
          "Show progress and cali-query performance summary",
          nullptr
        },
        { "caliper-config", "caliper-config", 0, true,
          "Caliper configuration flags (for cali-query profiling)",
          "KEY=VALUE,..."
        },
        { "verbose", "verbose", 'v', false,
          "Be verbose.",
          nullptr
        },
        { "output", "output", 'o', true,  "Set the output file name", "FILE"  },
        { "help",   "help",   'h', false, "Print help message",       nullptr },
        { "list-attributes", "list-attributes", 0, false,
          "Extract and list attributes in Caliper stream instead of snapshot records",
          nullptr
        },
        Args::Table::Terminator
    };

    /// A node record filter that filters redundant identical node records.
    /// Redundant node records can occur when merging/unifying two streams.
    class FilterDuplicateNodes {
        cali_id_t m_max_node;

    public:

        FilterDuplicateNodes()
            : m_max_node { 0 }
            { } 

        void operator()(CaliperMetadataAccessInterface& db, const Node* node, NodeProcessFn push) {
            cali_id_t id = node->id();

            if (id != CALI_INV_ID) {
                if (id < m_max_node) {
                    return;
                } else 
                    m_max_node = id;
            }

            push(db, node);
        }
    };


    /// NodeFilterStep helper struct
    /// Basically the chain link in the processing chain.
    /// Passes result of @param m_filter_fn to @param m_push_fn
    struct NodeFilterStep {
        NodeFilterFn  m_filter_fn; ///< This processing step
        NodeProcessFn m_push_fn;   ///< Next processing step

        NodeFilterStep(NodeFilterFn filter_fn, NodeProcessFn push_fn) 
            : m_filter_fn { filter_fn }, m_push_fn { push_fn }
            { }

        void operator ()(CaliperMetadataAccessInterface& db, const Node* node) {
            m_filter_fn(db, node, m_push_fn);
        }
    };

}


void setup_caliper_config(const Args& args)
{
    const char* progressmonitor_profile[][2] = {
        { "CALI_SERVICES_ENABLE", "event:report:textlog:trace:timestamp" },
        { "CALI_EVENT_TRIGGER",   "annotation:cali-query.stream"         },
        { "CALI_TEXTLOG_TRIGGER", "cali-query.stream" },

        { "CALI_TEXTLOG_FORMATSTRING",
          "cali-query: Processed %[52]cali-query.stream% (thread %[2]thread%): %[8]time.inclusive.duration% us" },

        { "CALI_REPORT_CONFIG",
          "SELECT annotation,time.inclusive.duration WHERE event.end#annotation FORMAT table" },

        { NULL, NULL }
    };
    
    cali_config_preset("CALI_LOG_VERBOSITY", "0");
    cali_config_preset("CALI_CALIPER_ATTRIBUTE_PROPERTIES", "annotation=process_scope:nested");

    cali_config_allow_read_env(false);

    cali_config_define_profile("caliquery-progressmonitor", progressmonitor_profile);

    cali_config_set("CALI_CONFIG_FILE", "cali-query_caliper.config");
    
    if (args.is_set("verbose"))
        cali_config_preset("CALI_LOG_VERBOSITY", "1");
    if (args.is_set("profile"))
        cali_config_set("CALI_CONFIG_PROFILE", "caliquery-progressmonitor");

    std::vector<std::string> config_list = 
        StringConverter(args.get("caliper-config")).to_stringlist();

    for (const std::string entry : config_list) {
        auto p = entry.find('=');

        if (p == std::string::npos) {
            std::cerr << "cali-query: error: invalid Caliper configuration flag format \"" 
                      << entry << "\" (missing \"=\")" << std::endl;
            continue;
        }

        cali_config_set(entry.substr(0, p).c_str(), entry.substr(p+1).c_str());
    }
}


//
// --- main()
//

int main(int argc, const char* argv[])
{

    Args args(::option_table);

    //
    // --- Parse command line arguments
    //

    {
        int i = args.parse(argc, argv);

        if (i < argc) {
            cerr << "cali-query: error: unknown option: " << argv[i] << '\n'
                 << "  Available options: ";

            args.print_available_options(cerr);
            
            return -1;
        }

        if (args.is_set("help")) {
            cerr << usage << "\n\n";

            args.print_available_options(cerr);

            return 0;
        }
    }

    bool verbose = args.is_set("verbose");
    
    // The Caliper config setup must run before Caliper runtime initialization
    setup_caliper_config(args);
    
    cali::Annotation("cali-query.build.date").set(__DATE__);
    cali::Annotation("cali-query.build.time").set(__TIME__);
#ifdef __GNUC__
    cali::Annotation("cali-query.build.compiler").set("gnu-" __VERSION__);
#endif 

    CALI_MARK_BEGIN("Initialization");

    //
    // --- Create output stream (if requested)
    //

    OutputStream stream;

    if (args.is_set("output"))
        stream.set_filename(args.get("output").c_str());
    else
        stream.set_stream(OutputStream::StdOut);

    //
    // --- Build up processing chain (from back to front)
    //

    QuerySpec         spec = spec_from_args(args);

    // setup format spec
    
    FormatProcessor   format(spec, stream);

    NodeProcessFn     node_proc = [](CaliperMetadataAccessInterface&,const Node*) { return; };
    SnapshotProcessFn snap_proc = [](CaliperMetadataAccessInterface&,const EntryList&){ return; };

    Aggregator        aggregate(spec);

    if (spec.aggregation_ops.selection == QuerySpec::AggregationSelection::None)
        snap_proc = format;
    else
        snap_proc = aggregate;
    
    if (spec.filter.selection == QuerySpec::FilterSelection::List)
        snap_proc = ::SnapshotFilterStep(RecordSelector(spec), snap_proc);
    
    if (args.is_set("list-attributes")) {
        node_proc = AttributeExtract(snap_proc);
        snap_proc = [](CaliperMetadataAccessInterface&,const EntryList&){ return; };
    }

    node_proc = ::NodeFilterStep(::FilterDuplicateNodes(), node_proc);

    std::vector<std::string> files = args.arguments();

    if (files.empty())
        files.push_back(""); // read from stdin if no files are given
    
    unsigned num_threads =
        std::min<unsigned>(files.size(), std::stoul(args.get("threads", "4")));

    if (verbose)
        std::cerr << "cali-query: Processing " << files.size()
                  << " files using "
                  << num_threads << " thread" << (num_threads == 1 ? "." : "s.")
                  << std::endl;

    Annotation("cali-query.num-threads", CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_SKIP_EVENTS).set(static_cast<int>(num_threads));

    CALI_MARK_END("Initialization");
    
    //
    // --- Thread processing function
    //

    CALI_MARK_BEGIN("Processing");

    CaliperMetadataDB     metadb;
    std::atomic<unsigned> index(0);
    std::mutex            msgmutex;
    
    auto thread_fn = [&](unsigned t) {
        Annotation::Guard
            g_t(Annotation("thread").set(static_cast<int>(t)));
        
        for (unsigned i = index++; i < files.size(); i = index++) { // "index++" is atomic read-mod-write
            const char* filename = (files[i].empty() ? "stdin" : files[i].c_str());
            
            Annotation::Guard 
                g_s(Annotation("cali-query.stream").begin(filename));

            if (verbose) {
                std::lock_guard<std::mutex>
                    g(msgmutex);

                std::cerr << "cali-query: Reading " << filename << std::endl;
            }
           
            CsvReader reader(files[i]);
            IdMap     idmap;

            if (!reader.read([&](const RecordMap& rec){
                        metadb.merge(rec, idmap, node_proc, snap_proc);
                    })) {
                std::lock_guard<std::mutex>
                    g(msgmutex);
                
                std::cerr << "cali-query: Error: Could not read file " << filename << std::endl;
            }
        }
    };

    std::vector<std::thread> threads;

    //
    // --- Fill thread vector and process
    //

    for (unsigned t = 0; t < num_threads; ++t)
        threads.emplace_back(thread_fn, t);

    for (auto &t : threads)
        t.join();

    CALI_MARK_END("Processing");
    
    //
    // --- Flush outputs
    //

    CALI_MARK_BEGIN("Writing");

    aggregate.flush(metadb, format);
    format.flush(metadb);

    CALI_MARK_END("Writing");
}

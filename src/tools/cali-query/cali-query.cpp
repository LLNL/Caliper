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

/// @file cali-query.cpp
/// A basic tool for Caliper metadata queries

#include <Args.h>

#include <Annotation.h>

#include <Aggregator.h>
#include <CaliperMetadataDB.h>
#include <Expand.h>
#include <Format.h>
#include <RecordProcessor.h>
#include <RecordSelector.h>
#include <Table.h>
#include <Json.h>

#include <ContextRecord.h>
#include <Node.h>

#include <csv/CsvReader.h>
#include <csv/CsvSpec.h>
#include <csv/CsvWriter.h>

#include <util/split.hpp>

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
          "Print given attributes in human-readable table form",
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
        { "output", "output", 'o', true,  "Set the output file name", "FILE"  },
        { "help",   "help",   'h', false, "Print help message",       nullptr },
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

    /// FilterStep helper struct
    /// Basically the chain link in the processing chain.
    /// Passes result of @param m_filter_fn to @param m_push_fn
    struct FilterStep {
        RecordFilterFn  m_filter_fn; ///< This processing step
        RecordProcessFn m_push_fn;   ///< Next processing step

        FilterStep(RecordFilterFn filter_fn, RecordProcessFn push_fn) 
            : m_filter_fn { filter_fn }, m_push_fn { push_fn }
            { }

        void operator ()(CaliperMetadataAccessInterface& db, const RecordMap& rec) {
            m_filter_fn(db, rec, m_push_fn);
        }
    };

    /// SnapshotFilterStep helper struct
    /// Basically the chain link in the processing chain.
    /// Passes result of @param m_filter_fn to @param m_push_fn
    struct SnapshotFilterStep {
        SnapshotFilterFn  m_filter_fn;  ///< This processing step
        SnapshotProcessFn m_push_fn;    ///< Next processing step

        SnapshotFilterStep(SnapshotFilterFn filter_fn, SnapshotProcessFn push_fn) 
            : m_filter_fn { filter_fn }, m_push_fn { push_fn }
            { }

        void operator ()(CaliperMetadataAccessInterface& db, const EntryList& list) {
            m_filter_fn(db, list, m_push_fn);
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


//
// --- main()
//

int main(int argc, const char* argv[])
{
    Annotation a_phase("cali-query.phase", CALI_ATTR_SCOPE_PROCESS);

    Annotation::Guard g_p(a_phase);

    a_phase.set("init");

    cali::Annotation("cali-query.build.date").set(__DATE__);
    cali::Annotation("cali-query.build.time").set(__TIME__);
#ifdef __GNUC__
    cali::Annotation("cali-query.build.compiler").set("gnu-" __VERSION__);
#endif 

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

    //
    // --- Create output stream (if requested)
    //

    ofstream fs;

    if (args.is_set("output")) {
        string filename = args.get("output");

        fs.open(filename.c_str());

        if (!fs) {
            cerr << "cali-query: error: could not open output file " 
                 << filename << endl;

            return -2;
        } 
    }

    //
    // --- Build up processing chain (from back to front)
    //

    Table             tbl_writer(args.get("attributes"), args.get("sort"));
    Json              jsn_writer(args.get("attributes"));

    NodeProcessFn     node_proc   = [](CaliperMetadataAccessInterface&,const Node*) { return; };
    SnapshotProcessFn snap_writer = [](CaliperMetadataAccessInterface&,const EntryList&){ return; };


    // differentiate between "expand" and "format"
    if (args.is_set("expand")) {
        snap_writer = Expand(fs.is_open() ? fs : cout, args.get("attributes"));
    } else if (args.is_set("format")) {
        string formatstr = args.get("format");
        
        if (formatstr.empty()) {
            cerr << "cali-query: Format string required for --format" << endl;
            return -2;
        }
        
        snap_writer = Format(fs.is_open() ? fs : cout, formatstr, args.get("title"));
    } else if (args.is_set("table")) {        
        snap_writer = tbl_writer;
    } 
    else if(args.is_set("json")) {
        snap_writer = jsn_writer;
    }
    else {
        CsvWriter writer(fs.is_open() ? fs : cout);

        snap_writer = writer;
        node_proc   = writer;
    }

    Aggregator        aggregate(args.get("aggregate"), args.get("aggregate-key"));
    SnapshotProcessFn snap_proc(args.is_set("aggregate") ? aggregate : snap_writer);

    string select = args.get("select");

    if (!select.empty())
        snap_proc = ::SnapshotFilterStep(RecordSelector(select), snap_proc);
    else if (args.is_set("select"))
        cerr << "cali-query: Arguments required for --select" << endl;

    node_proc = ::NodeFilterStep(::FilterDuplicateNodes(), node_proc);

    std::vector<std::string> files = args.arguments();

    if (files.empty())
        files.push_back(""); // read from stdin if no files are given
    
    unsigned num_threads =
        std::min<unsigned>(files.size(), std::stoul(args.get("threads", "4")));

    std::cerr << "cali-query: processing " << files.size() << " files using "
              << num_threads << " thread" << (num_threads == 1 ? "." : "s.")  << std::endl;

    Annotation("cali-query.num-threads", CALI_ATTR_SCOPE_PROCESS).set(static_cast<int>(num_threads));
    
    //
    // --- Thread processing function
    //

    a_phase.set("process");

    CaliperMetadataDB     metadb;
    std::atomic<unsigned> index(0);
    
    auto thread_fn = [&](unsigned t) {
        Annotation::Guard
            g_t(Annotation("thread").set(static_cast<int>(t)));
        
        for (unsigned i = index++; i < files.size(); i = index++) { // "index++" is atomic read-mod-write 
            Annotation::Guard 
                g_s(Annotation("cali-query.stream").set(files[i].empty() ? "stdin" : files[i].c_str()));
            
            CsvReader reader(files[i]);
            IdMap     idmap;

            if (!reader.read([&](const RecordMap& rec){ metadb.merge(rec, idmap, node_proc, snap_proc); }))
                cerr << "Could not read file " << files[i] << endl;
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
    
    //
    // --- Flush outputs
    //

    a_phase.set("flush");

    aggregate.flush(metadb, snap_writer);

    if (args.is_set("table"))
        tbl_writer.flush(metadb, fs.is_open() ? fs : cout);
    if (args.is_set("json"))
        jsn_writer.flush(metadb, fs.is_open() ? fs : cout);
}

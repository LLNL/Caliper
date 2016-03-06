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

/// @file cali-query.cpp
/// A basic tool for Caliper metadata queries

#include <Args.h>

#include <Annotation.h>

#include <Aggregator.h>
#include <CaliperMetadataDB.h>
#include <Expand.h>
#include <RecordProcessor.h>
#include <RecordSelector.h>

#include <ContextRecord.h>
#include <Node.h>

#include <csv/CsvReader.h>
#include <csv/CsvSpec.h>

#include <util/split.hpp>

#include <fstream>
#include <iostream>
#include <iterator>

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
          "Select context records: [-]attribute[(<|>|=)value][:...]", 
          "QUERY_STRING" 
        },
        { "expand", "expand", 'e', false,  
          "Expand context records and print the selected attributes (default: all)", 
          nullptr 
        },
        { "attributes", "print-attributes", 0, true,
          "Select attributes to print, or hide: [-]attribute[:...]",
          "ATTRIBUTES"
        },
        { "aggregate", "aggregate", 'a', true,
          "Aggregate the given attributes",
          "ATTRIBUTES"
        },
        { "output", "output", 'o', true,  "Set the output file name", "FILE"  },
        { "help",   "help",   'h', false, "Print help message",       nullptr },
        Args::Table::Terminator
    };

    class WriteRecord {
        ostream&      m_os;

    public:
        
        WriteRecord(ostream& os)
            : m_os(os) { }

        void operator()(CaliperMetadataDB&, const Node* node) {
            node->push_record([this](const RecordDescriptor& r,const int* c,const Variant** d)
                              { CsvSpec::write_record(m_os, r, c, d); });
        }

        void operator()(CaliperMetadataDB&, const EntryList& list) {
            std::vector<Variant> attr;
            std::vector<Variant> vals;
            std::vector<Variant> refs;

            for (const Entry& e : list) {
                if (e.node()) {
                    refs.push_back(Variant(e.node()->id()));
                } else if (e.attribute() != CALI_INV_ID) {
                    attr.push_back(Variant(e.attribute()));
                    vals.push_back(e.value());
                }
            }

            int           count[3] = { static_cast<int>(refs.size()),
                                       static_cast<int>(attr.size()),
                                       static_cast<int>(vals.size()) };
            const Variant* data[3] = { &refs.front(), &attr.front(), &vals.front() };

            CsvSpec::write_record(m_os, ContextRecord::record_descriptor(), count, data);
        }

        void operator()(CaliperMetadataDB& /* cb */, const RecordMap& rec) {
            m_os << rec << endl;
        }
    };

    /// A node record filter that filters redundant identical node records.
    /// Redundant node records can occur when merging/unifying two streams.
    class FilterDuplicateNodes {
        cali_id_t m_max_node;

    public:

        FilterDuplicateNodes()
            : m_max_node { 0 }
            { } 

        void operator()(CaliperMetadataDB& db, const Node* node, NodeProcessFn push) {
            cali_id_t id = node->id();

            if (id != CALI_INV_ID) {
                if (id < m_max_node) {
                    return;
                } else 
                    m_max_node = id;
            }

            push(db, node);
        }
        
        void operator()(CaliperMetadataDB& db, const RecordMap& rec, RecordProcessFn push) {
            if (get_record_type(rec) == "node") {
                auto id_entry_it = rec.find("id");

                if (id_entry_it != rec.end() && !id_entry_it->second.empty()) {
                    cali_id_t id = id_entry_it->second.front().to_id();

                    if (id != CALI_INV_ID) {
                        if (id < m_max_node)
                            return;
                        else
                            m_max_node = id;
                    }
                }                
            }

            push(db, rec);
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

        void operator ()(CaliperMetadataDB& db, const RecordMap& rec) {
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

        void operator ()(CaliperMetadataDB& db, const EntryList& list) {
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

        void operator ()(CaliperMetadataDB& db, const Node* node) {
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

    NodeProcessFn     node_proc   = [](CaliperMetadataDB&,const Node*) { return; };
    SnapshotProcessFn snap_writer = [](CaliperMetadataDB&,const EntryList&){ return; };

    if (args.is_set("expand"))
        snap_writer = Expand(fs.is_open() ? fs : cout, args.get("attributes"));
    else {
        WriteRecord writer = WriteRecord(fs.is_open() ? fs : cout);

        snap_writer = writer;
        node_proc   = writer;
    }

    Aggregator        aggregate { args.get("aggregate") };
    SnapshotProcessFn snap_proc { args.is_set("aggregate") ? aggregate : snap_writer };

    string select = args.get("select");

    if (!select.empty())
        snap_proc = ::SnapshotFilterStep(RecordSelector(select), snap_proc);
    else if (args.is_set("select"))
        cerr << "cali-query: Arguments required for --select" << endl;

    node_proc = ::NodeFilterStep(::FilterDuplicateNodes(), node_proc);


    //
    // --- Process inputs
    //

    a_phase.set("process");

    CaliperMetadataDB metadb;

    for (const string& file : args.arguments()) {
        Annotation::Guard 
            g_s(Annotation("cali-query.stream").set(file.c_str()));
            
        CsvReader reader(file);
        IdMap     idmap;

        if (!reader.read([&](const RecordMap& rec){ metadb.merge(rec, idmap, node_proc, snap_proc); }))
            cerr << "Could not read file " << file << endl;
    }

    a_phase.set("flush");

    aggregate.flush(metadb, snap_writer);
}

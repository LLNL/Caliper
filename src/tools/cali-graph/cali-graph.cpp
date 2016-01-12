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
        "\n  Export generalized context tree as graphiz (.dot) file";

    const Args::Table option_table[] = { 
        // name, longopt name, shortopt char, has argument, info, argument info
        { "max", "max", 'm', true,  
          "Export at most this many nodes", 
          "NUMBER_OF_NODES" 
        },
        { "output", "output", 'o', true,  "Set the output file name", "FILE"  },
        { "help",   "help",   'h', false, "Print help message",       nullptr },
        Args::Table::Terminator
    };

    class DotPrinter {
        ostream& m_os;
        int      m_max;
        
    public:

        DotPrinter(ostream& os, int max = -1)
            : m_os(os), m_max(max)
            { }

        void print_prefix() {
            m_os << "graph {" << endl;
        }

        void print_postfix() {
            m_os << "}" << endl;
        }

        void print_node(CaliperMetadataDB& db, const Node* node) {
            if (!node || (m_max >= 0 && node->id() > static_cast<cali_id_t>(m_max)))
                return;

            Attribute attr = db.attribute(node->attribute());

            m_os << "  " << node->id()
                 << " [label=\"" << attr.name() << ":" << node->data().to_string() << "\"];"
                 << endl;

            if (node->parent())
                m_os << "  " << node->parent()->id() << " -- " << node->id() << ";"
                     << endl;
        }

        void operator()(CaliperMetadataDB& db, const RecordMap& rec) {
            if (get_record_type(rec) == "node") {
                auto id_entry_it = rec.find("id");

                if (id_entry_it != rec.end() && !id_entry_it->second.empty())
                    print_node(db, db.node(id_entry_it->second.front().to_id()));
            }
        }
    };
    

    /// A node record filter that filters redundant identical node records.
    /// Redundant node records can occur when merging/unifying two streams.
    class FilterDuplicateNodes {
        cali_id_t       m_max_node;

    public:

        FilterDuplicateNodes()
            : m_max_node { 0 }
            { } 
        
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
}


//
// --- main()
//

int main(int argc, const char* argv[])
{
    Annotation a_phase("cali-graph.phase", CALI_ATTR_SCOPE_PROCESS);

    Annotation::Guard g_p(a_phase);

    a_phase.set("init");

    Args args(::option_table);
    
    //
    // --- Parse command line arguments
    //

    int max = -1;
    
    {
        int i = args.parse(argc, argv);

        if (i < argc) {
            cerr << "cali-graph: error: unknown option: " << argv[i] << '\n'
                 << "  Available options: ";

            args.print_available_options(cerr);
            
            return -1;
        }

        if (args.is_set("help")) {
            cerr << usage << "\n\n";

            args.print_available_options(cerr);

            return 0;
        }

        if (args.is_set("max"))
            max = stoi(args.get("max"));
    }

    //
    // --- Create output stream (if requested)
    //

    ofstream fs;

    if (args.is_set("output")) {
        string filename = args.get("output");

        fs.open(filename.c_str());

        if (!fs) {
            cerr << "cali-graph: error: could not open output file " 
                 << filename << endl;

            return -2;
        } 
    }

    //
    // --- Build up processing chain (from back to front)
    //

    DotPrinter      dotprint(fs.is_open() ? fs : cout, max);
    RecordProcessFn processor = dotprint;

    processor = ::FilterStep(::FilterDuplicateNodes(), processor);

    //
    // --- Process inputs
    //

    a_phase.set("process");

    dotprint.print_prefix();
    
    CaliperMetadataDB metadb;

    for (const string& file : args.arguments()) {
        Annotation::Guard 
            g_s(Annotation("cali-graph.stream").set(file.c_str()));
            
        CsvReader reader(file);
        IdMap     idmap;

        if (!reader.read([&](const RecordMap& rec){ processor(metadb, metadb.merge(rec, idmap)); }))
            cerr << "Could not read file " << file << endl;
    }

    dotprint.print_postfix();
}

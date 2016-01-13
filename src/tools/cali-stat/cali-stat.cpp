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

/// @file cali-stat.cpp
/// A tool that quantifies Caliper stream contents

#include <Args.h>

#include <Annotation.h>

#include <Aggregator.h>
#include <CaliperMetadataDB.h>
#include <RecordProcessor.h>

#include <ContextRecord.h>
#include <Node.h>

#include <csv/CsvReader.h>

#include <util/split.hpp>

#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <limits>

using namespace cali;
using namespace std;
using namespace util;

namespace
{
    const char* usage = "cali-stat [OPTION]... [FILE]..."
        "\n  Collect and print statistics about data elements in Caliper streams";

    const Args::Table option_table[] = { 
        // name, longopt name, shortopt char, has argument, info, argument info
        { "output", "output", 'o', true,  "Set the output file name", "FILE"  },
        { "help",   "help",   'h', false, "Print help message",       nullptr },
        Args::Table::Terminator
    };

    class CaliStreamStat {
        struct S {
            int n_snapshots;
            int n_nodes;

            int n_max_snapshot; // max number of elements in snapshot record
            int n_min_snapshot; // min number of elements in snapshot record
            
            int n_ref;          // number of tree reference elements
            int n_val;          // number of immediate data elements
            int n_tot;          // number of total data elements in ctx records

            int n_attr_refs;    // number of attributes in snapshot records
        };

        std::shared_ptr<S> mS;
        
    public:

        CaliStreamStat()
            : mS(new S { 0, 0, 0, std::numeric_limits<int>::max(), 0, 0, 0 } )
            { }

        void print_results(ostream& os) {
            os << "Number of records\n"
               << "Total          Nodes          Snapshots\n"
               << std::left
               << std::setw(15) << mS->n_snapshots + mS->n_nodes
               << std::setw(15) << mS->n_nodes
               << std::setw(15) << mS->n_snapshots
               << endl;

            os << "\nData elements\n"
               << "Total          Nodes          Tree refs      Direct values\n"
               << std::setw(15) << mS->n_tot + 4 * mS->n_nodes
               << std::setw(15) << 4 * mS->n_nodes
               << std::setw(15) << mS->n_ref
               << std::setw(15) << 2 * mS->n_val
               << endl;

            if (mS->n_snapshots < 1)
                return;
            
            os << "\nSnapshot record size\n"
               << "Min            Max            Average\n"
               << std::setw(15) << mS->n_min_snapshot
               << std::setw(15) << mS->n_max_snapshot
               << std::setw(15) << static_cast<double>(mS->n_tot) / mS->n_snapshots
               << endl;
            
            os << "\nAttributes referenced in snapshot records\n"
               << "Total          Average        Attr/Elem\n"
               << std::setw(15) << mS->n_attr_refs
               << std::setw(15) << static_cast<double>(mS->n_attr_refs) / mS->n_snapshots
               << std::setw(15) << static_cast<double>(mS->n_attr_refs) / (mS->n_tot + 4 * mS->n_nodes)
               << endl;
        }

        void process_snapshot(const CaliperMetadataDB& db, const RecordMap& rec) {
            ++mS->n_snapshots;
            
            auto ref_entry_it = rec.find("ref");
            auto val_entry_it = rec.find("attr");

            int ref = ref_entry_it == rec.end() ? 0 : static_cast<int>(ref_entry_it->second.size());
            int val = val_entry_it == rec.end() ? 0 : static_cast<int>(val_entry_it->second.size());

            int ref_attr = 0;
            
            if (ref_entry_it != rec.end())
                for ( const Variant& ref_node_id : ref_entry_it->second )
                    for (const Node* node = db.node(ref_node_id.to_id()); node; node = node->parent())
                        ++ref_attr;

            if (ref_attr)
                --ref_attr; // don't count hidden root node
                    
            mS->n_ref += ref;
            mS->n_val += val;
            mS->n_tot += ref + 2*val;

            mS->n_min_snapshot = std::min(mS->n_min_snapshot, ref + 2*val);
            mS->n_max_snapshot = std::max(mS->n_max_snapshot, ref + 2*val);

            mS->n_attr_refs += ref_attr + val;
        }        

        void operator()(CaliperMetadataDB& db, const RecordMap& rec) {
            string type = get_record_type(rec);
            
            if      (type == "node")
                ++mS->n_nodes;
            else if (type == "ctx")
                process_snapshot(db, rec);
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
    Annotation a_phase("cali-stat.phase", CALI_ATTR_SCOPE_PROCESS);

    Annotation::Guard g_p(a_phase);

    a_phase.set("init");

    Args args(::option_table);
    
    //
    // --- Parse command line arguments
    //
    
    {
        int i = args.parse(argc, argv);

        if (i < argc) {
            cerr << "cali-stat: error: unknown option: " << argv[i] << '\n'
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
            cerr << "cali-stat: error: could not open output file " 
                 << filename << endl;

            return -2;
        } 
    }

    //
    // --- Build up processing chain (from back to front)
    //

    ::CaliStreamStat stat;
    RecordProcessFn  processor = stat;

    processor = ::FilterStep(::FilterDuplicateNodes(), processor);

    //
    // --- Process inputs
    //

    a_phase.set("process");

    CaliperMetadataDB metadb;

    for (const string& file : args.arguments()) {
        Annotation::Guard 
            g_s(Annotation("cali-stat.stream").set(file.c_str()));
            
        CsvReader reader(file);
        IdMap     idmap;

        if (!reader.read([&](const RecordMap& rec){ processor(metadb, metadb.merge(rec, idmap)); }))
            cerr << "Could not read file " << file << endl;
    }

    stat.print_results(fs.is_open() ? fs : cout);
}

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
#include <sstream>

using namespace cali;
using namespace std;
using namespace util;

namespace
{
    const char* usage = "cali-stat [OPTION]... [FILE]..."
        "\n  Collect and print statistics about data elements in Caliper streams";

    const Args::Table option_table[] = { 
        // name, longopt name, shortopt char, has argument, info, argument info
        { "reuse",  "reuse-statistics", 'r', false,
          "Print tree data reuse statistics", nullptr
        },
        { "output", "output", 'o', true,  "Set the output file name", "FILE"  },
        { "help",   "help",   'h', false, "Print help message",       nullptr },
        Args::Table::Terminator
    };

    class ReuseStat {
        struct S {
            struct ReuseInfo {
                int              nodes; // number of nodes with this attribute
                map<string, int> data;  // different data elements 
            };
            
            std::map< cali_id_t, ReuseInfo > reuse;
        };

        std::shared_ptr<S> mS;

    public:

        ReuseStat()
            : mS(new S)
            { }

        void print_results(CaliperMetadataDB& db, ostream& os) {
            os << "\nReuse statistics:\n"
               << "Attribute                       #nodes      #elem       #uses       #uses/elem  #uses/node\n";

            for (auto &p : mS->reuse) {
                int nelem = static_cast<int>(p.second.data.size());
                
                os << std::setw(32) << db.attribute(p.first).name()
                   << std::setw(12) << p.second.nodes
                   << std::setw(12) << nelem;

                double total_uses = 0.0;

                for ( auto &dp : p.second.data )
                    total_uses += dp.second;

                os << std::setw(12) << total_uses
                   << std::setw(12) << (nelem > 0 ? total_uses / nelem : 0.0)
                   << std::setw(12) << (total_uses / p.second.nodes)
                   << endl;
            }
                
        }

        void process_node(CaliperMetadataDB& db, const Node* node) {
            auto it = mS->reuse.find(node->attribute());

            if (it == mS->reuse.end()) {
                S::ReuseInfo info;

                info.nodes = 1;
                info.data[node->data().to_string()] = 1;

                mS->reuse.insert(make_pair(node->attribute(), info));
            } else {
                ++(it->second.nodes);
                ++(it->second.data[node->data().to_string()]);
            }
        }
        
        void process_snapshot(const CaliperMetadataDB& db, const RecordMap& rec) {
            auto ref_entry_it = rec.find("ref");

            int ref = ref_entry_it == rec.end() ? 0 : static_cast<int>(ref_entry_it->second.size());

            if (ref_entry_it != rec.end())
                for ( const Variant& ref_node_id : ref_entry_it->second )
                    for (const Node* node = db.node(ref_node_id.to_id()); node; node = node->parent()) {
                        auto it = mS->reuse.find(node->attribute());

                        if (it != mS->reuse.end())
                            ++(it->second.data[node->data().to_string()]);
                    }   
        }

        void operator()(CaliperMetadataDB& db, const RecordMap& rec) {
            string type = get_record_type(rec);
            
            if      (type == "node") {
                auto id_entry_it = rec.find("id");

                if (id_entry_it != rec.end() && !id_entry_it->second.empty())
                    process_node(db, db.node(id_entry_it->second.front().to_id()));
            } else if (type == "ctx")
                process_snapshot(db, rec);
        }
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

            int size_nodes;     // (est.) size of all node records
            int size_snapshots; // (est.) size of all snapshot records
        };

        std::shared_ptr<S> mS;

        string format_size(double size) {
            ostringstream os;

            const char* postfix[] = { "", "KiB", "MiB", "GiB" };
            int p = 0;
            
            for ( ; size > 1024 && p < 3; ++p)
                size /= 1024;

            os << size << postfix[p];

            return os.str();
        }
        
    public:

        CaliStreamStat()
            : mS(new S { 0, 0, 0, std::numeric_limits<int>::max(), 0, 0, 0, 0, 0, 0 } )
            { }

        void print_results(ostream& os) {
            os << "Number of records\n"
               << "Total          Nodes          Snapshots\n"
               << std::left
               << std::setw(15) << mS->n_snapshots + mS->n_nodes
               << std::setw(15) << mS->n_nodes
               << std::setw(15) << mS->n_snapshots
               << endl;

            os << "\nNumber of elements\n"
               << "Total          Nodes          Tree refs      Direct val\n"
               << std::setw(15) << mS->n_tot + 4 * mS->n_nodes
               << std::setw(15) << 4 * mS->n_nodes
               << std::setw(15) << mS->n_ref
               << std::setw(15) << 2 * mS->n_val
               << endl;

            os << "\nData size (est.)\n"
               << "Total          Nodes          Snapshots\n"
               << std::setw(15) << format_size(mS->size_nodes + mS->size_snapshots)
               << std::setw(15) << format_size(mS->size_nodes)
               << std::setw(15) << format_size(mS->size_snapshots)
               << endl;
            
            if (mS->n_snapshots < 1)
                return;
            
            os << "\nElements/snapshot\n"
               << "Min            Max            Average\n"
               << std::setw(15) << mS->n_min_snapshot
               << std::setw(15) << mS->n_max_snapshot
               << std::setw(15) << static_cast<double>(mS->n_tot) / mS->n_snapshots
               << endl;
            
            os << "\nAttributes referenced in snapshot records\n"
               << "Total          Average        Refs/Elem\n"
               << std::setw(15) << mS->n_attr_refs
               << std::setw(15) << static_cast<double>(mS->n_attr_refs) / mS->n_snapshots
               << std::setw(15) << static_cast<double>(mS->n_attr_refs) / (mS->n_tot + 4 * mS->n_nodes)
               << endl;
        }

        void process_node(CaliperMetadataDB& db, const Node* node) {
            ++mS->n_nodes;

            // Get string size for usr and string nodes, otherwise assume 8 bytes
            cali_attr_type type = db.attribute(node->attribute()).type();
            
            mS->size_nodes += 3 * 8 +
                (type == CALI_TYPE_USR || type == CALI_TYPE_STRING ? node->data().to_string().size() : 8);
        }

        void process_snapshot(const CaliperMetadataDB& db, const RecordMap& rec) {
            ++mS->n_snapshots;
            
            auto ref_entry_it  = rec.find("ref");
            auto attr_entry_it = rec.find("attr");
            auto val_entry_it  = rec.find("data");
            
            int ref = ref_entry_it == rec.end() ? 0 : static_cast<int>(ref_entry_it->second.size());
            int val = val_entry_it == rec.end() ? 0 : static_cast<int>(val_entry_it->second.size());

            int ref_attr = 0;
            
            if (ref_entry_it != rec.end())
                for ( const Variant& ref_node_id : ref_entry_it->second )
                    for (const Node* node = db.node(ref_node_id.to_id()); node && node->id() != CALI_INV_ID; node = node->parent())
                        ++ref_attr;
                    
            mS->n_ref += ref;
            mS->n_val += val;
            mS->n_tot += ref + 2*val;

            mS->n_min_snapshot = std::min(mS->n_min_snapshot, ref + 2*val);
            mS->n_max_snapshot = std::max(mS->n_max_snapshot, ref + 2*val);

            mS->n_attr_refs += ref_attr + val;

            mS->size_snapshots += ref * 8;
            
            for (int i = 0; i < val; ++i) {
                cali_attr_type type = db.attribute(attr_entry_it->second[i].to_id()).type();

                mS->size_snapshots +=
                    (type == CALI_TYPE_USR || type == CALI_TYPE_STRING ? val_entry_it->second[i].to_string().size() : 8);
            }
        }        

        void operator()(CaliperMetadataDB& db, const RecordMap& rec) {
            string type = get_record_type(rec);
            
            if      (type == "node") {
                auto id_entry_it = rec.find("id");

                if (id_entry_it != rec.end() && !id_entry_it->second.empty())
                    process_node(db, db.node(id_entry_it->second.front().to_id()));
            } else if (type == "ctx")
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

    struct MultiProcessor {
        vector<RecordProcessFn> m_processors;

        void add(RecordProcessFn fn) {
            m_processors.push_back(fn);
        }

        void operator()(CaliperMetadataDB& db, const RecordMap& rec) {
            for (auto &f : m_processors)
                f(db, rec);
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

    ::ReuseStat      reuse_stat;
    ::CaliStreamStat stream_stat;
    
    MultiProcessor   stats;
    
    if (args.is_set("reuse"))
        stats.add(reuse_stat);

    stats.add(stream_stat);

    RecordProcessFn  processor = ::FilterStep(::FilterDuplicateNodes(), stats);

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

    stream_stat.print_results(fs.is_open() ? fs : cout);

    if (args.is_set("reuse"))
        reuse_stat.print_results(metadb, fs.is_open() ? fs : cout);
}

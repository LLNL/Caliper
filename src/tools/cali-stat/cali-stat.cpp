// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// A tool that quantifies Caliper stream contents

#include "caliper/Annotation.h"

#include "caliper/tools-util/Args.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CaliReader.h"
#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/RecordProcessor.h"

#include "caliper/common/Node.h"
#include "caliper/common/StringConverter.h"

#include "caliper/common/util/split.hpp"

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

        void print_results(CaliperMetadataAccessInterface& db, ostream& os) {
            os << "\nReuse statistics:\n"
               << "Attribute                       #nodes      #elem       #uses       #uses/elem  #uses/node\n";

            for (auto &p : mS->reuse) {
                int nelem = static_cast<int>(p.second.data.size());
                
                os << std::setw(32) << db.get_attribute(p.first).name()
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

        void process_node(CaliperMetadataAccessInterface& db, const Node* node) {
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
        
        void process_rec(const CaliperMetadataAccessInterface& db, const EntryList& rec) {
            for (const Entry& e : rec)
                if (e.is_reference())
                    for (const Node* node = e.node(); node; node = node->parent()) {
                        auto it = mS->reuse.find(node->attribute());

                        if (it != mS->reuse.end())
                            ++(it->second.data[node->data().to_string()]);
                    }
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

        void process_node(CaliperMetadataAccessInterface& db, const Node* node) {
            ++mS->n_nodes;

            // Get string size for usr and string nodes, otherwise assume 8 bytes
            cali_attr_type type = db.get_attribute(node->attribute()).type();
            
            mS->size_nodes += 3 * 8 +
                (type == CALI_TYPE_USR || type == CALI_TYPE_STRING ? node->data().size() : 8);
        }

        void process_rec(const CaliperMetadataAccessInterface& db, const EntryList& rec) {
            ++mS->n_snapshots;
            
            int ref = 0;
            int imm = 0;

            int ref_attr = 0;

            for (const Entry& e : rec) {
                if (e.is_immediate()) {
                    ++imm;
                } else if (e.is_reference()) {
                    ++ref;
                    for (const Node* node = e.node(); node && node->id() != CALI_INV_ID; node = node->parent())
                        ++ref_attr;
                }
            }
            
            mS->n_ref += ref;
            mS->n_val += imm;
            mS->n_tot += ref + 2*imm;

            mS->n_min_snapshot = std::min(mS->n_min_snapshot, ref + 2*imm);
            mS->n_max_snapshot = std::max(mS->n_max_snapshot, ref + 2*imm);

            mS->n_attr_refs += ref_attr + imm;

            mS->size_snapshots += ref * 8;
        }        
    };    

    struct Processor {
        CaliStreamStat stream_stat;
        ReuseStat      reuse_stat;

        bool           do_reuse_stat;
        
        void operator()(CaliperMetadataAccessInterface& db, const Node* node) {
            stream_stat.process_node(db, node);

            if (do_reuse_stat)
                reuse_stat.process_node(db, node);
        }
        
        void operator()(CaliperMetadataAccessInterface& db, const EntryList& rec) {
            stream_stat.process_rec(db, rec);

            if (do_reuse_stat)
                reuse_stat.process_rec(db, rec);
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

    Processor processor;
    
    processor.do_reuse_stat = args.is_set("reuse");

    //
    // --- Process inputs
    //

    a_phase.set("process");

    CaliperMetadataDB metadb;

    for (const string& file : args.arguments()) {
        Annotation::Guard 
            g_s(Annotation("cali-stat.stream").set(file.c_str()));
            
        CaliReader reader(file);

        if (!reader.read(metadb, processor, processor))
            cerr << "Could not read file " << file << endl;
    }

    processor.stream_stat.print_results(fs.is_open() ? fs : cout);

    if (args.is_set("reuse"))
        processor.reuse_stat.print_results(metadb, fs.is_open() ? fs : cout);
}

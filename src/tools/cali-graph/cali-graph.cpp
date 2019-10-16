// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// A tool to print Caliper generalized context trees as graphviz files

#include "caliper/tools-util/Args.h"

#include "caliper/Annotation.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CaliReader.h"
#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/RecordProcessor.h"

#include "caliper/common/Node.h"

#include "caliper/common/util/split.hpp"

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
        { "max", "max-nodes", 'n', true,  
          "Export at most this many nodes", 
          "NUMBER_OF_NODES" 
        },
        { "skip-attribute-prefixes", "skip-attribute-prefixes", 0, false,
          "Skip attribute prefixes in nodes", nullptr 
        },
        { "output", "output", 'o', true,  "Set the output file name", "FILE"  },
        { "help",   "help",   'h', false, "Print help message",       nullptr },
        Args::Table::Terminator
    };

    class DotPrinter {
        ostream& m_os;
        Args     m_args;
        
        int      m_max;
        bool     m_skip_attr_prefixes;
        
        void parse_args(const Args& args) {
            if (args.is_set("max"))
                m_max = stoi(args.get("max"));
            if (args.is_set("skip-attribute-prefixes"))
                m_skip_attr_prefixes = true;
        }

        string format_attr_name(const Attribute& attr) {
            string name = attr.name();

            if (m_skip_attr_prefixes) {
                string::size_type n = name.rfind('.');

                if (n != string::npos)
                    name.erase(0, n+1);
            }

            return name;
        }
        
    public:

        DotPrinter(ostream& os, const Args& args)
            : m_os(os), m_args(args), m_max(-1), m_skip_attr_prefixes(false)
            {
                parse_args(args);
            }

        void print_prefix() {
            m_os << "graph {" << endl;
        }

        void print_postfix() {
            m_os << "}" << endl;
        }

        void print_node(CaliperMetadataAccessInterface& db, const Node* node) {
            if (!node || (m_max >= 0 && node->id() >= static_cast<cali_id_t>(m_max)))
                return;

            Attribute attr = db.get_attribute(node->attribute());

            m_os << "  " << node->id()
                 << " [label=\"" << format_attr_name(attr) << ":" << node->data().to_string() << "\"];"
                 << endl;

            if (node->parent() && node->parent()->id() != CALI_INV_ID)
                m_os << "  " << node->parent()->id() << " -- " << node->id() << ";"
                     << endl;
        }

        void operator()(CaliperMetadataAccessInterface& db, const Node* node) {
            print_node(db, node);
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


    DotPrinter        dotprint(fs.is_open() ? fs : cout, args);

    NodeProcessFn     node_proc = dotprint;
    SnapshotProcessFn snap_proc = [](CaliperMetadataAccessInterface&,const EntryList&){ return; };

    node_proc = ::NodeFilterStep(::FilterDuplicateNodes(), node_proc);

    //
    // --- Process inputs
    //

    a_phase.set("process");

    dotprint.print_prefix();
    
    CaliperMetadataDB metadb;

    for (const string& file : args.arguments()) {
        Annotation::Guard 
            g_s(Annotation("cali-graph.stream").set(file.c_str()));
            
        CaliReader reader(file);

        if (!reader.read(metadb, node_proc, snap_proc))
            cerr << "Could not read file " << file << endl;
    }

    dotprint.print_postfix();
}

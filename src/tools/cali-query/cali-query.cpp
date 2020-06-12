// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// A basic tool for Caliper metadata queries

#include "caliper/caliper-config.h"

#include "AttributeExtract.h"
#include "query_common.h"

#include "caliper/tools-util/Args.h"

#include "caliper/cali.h"
#include "caliper/cali-manager.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CaliReader.h"
#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/Preprocessor.h"
#include "caliper/reader/RecordProcessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/StringConverter.h"

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
        { "aggregate", "aggregate", 0, true,
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
    	{ "title",  "title",  0, true,
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
        { "caliper-config", "caliper-config", 'P', true,
          "Set Caliper configuration for profiling cali-query",
          "CALIPER-CONFIG"
        },
        { "caliper-config-vars", "caliper-config-vars", 0, true,
          "Caliper configuration flags (for cali-query profiling)",
          "KEY=VALUE,..."
        },
        { "verbose", "verbose", 'v', false, "Be verbose.",              nullptr },
        { "version", "version", 'V', false, "Print version number",     nullptr },
        { "output",  "output",  'o', true,  "Set the output file name", "FILE"  },
        { "help",    "help",    'h', true,  "Print help message",       nullptr },
        { "list-attributes", "list-attributes", 0, false,
          "Extract and list attributes in Caliper stream instead of snapshot records",
          nullptr
        },
        { "list-globals", "list-globals", 'G', false,
          "Extract and list global per-run attributes",
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

const char* progress_config_spec =
    "{"
    " \"name\"        : \"caliquery-progress\","
    " \"description\" : \"Print cali-query progress (when processing multiple files)\","
    " \"services\"    : [ \"event\", \"textlog\", \"timestamp\" ],"
    " \"config\"      : {"
    "   \"CALI_CHANNEL_FLUSH_ON_EXIT\" : \"false\","
    "   \"CALI_TIMER_UNIT\"            : \"sec\","
    "   \"CALI_EVENT_TRIGGER\"         : \"cali-query.stream\","
    "   \"CALI_TEXTLOG_TRIGGER\"       : \"cali-query.stream\","
    "   \"CALI_TEXTLOG_FILENAME\"      : \"stderr\","
    "   \"CALI_TEXTLOG_FORMATSTRING\"  : "
    "     \"cali-query: Processed %[52]cali-query.stream% (thread %[2]thread%): %[6]time.inclusive.duration% us\""
    "  }"
    "}";

void setup_caliper_config(const Args& args)
{
    //   Configure the default config, which can be provided by the user through
    // the "cali-query_caliper.config" file or the "caliper-config" command line arg

    cali_config_preset("CALI_LOG_VERBOSITY", "0");
    cali_config_preset("CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE", "process");

    cali_config_allow_read_env(false);
    cali_config_set("CALI_CONFIG_FILE", "cali-query_caliper.config");

    if (args.is_set("verbose"))
        cali_config_preset("CALI_LOG_VERBOSITY", "1");

    std::vector<std::string> config_list =
        StringConverter(args.get("caliper-config-vars")).to_stringlist();

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

    // The Caliper config setup must run before Caliper runtime initialization
    setup_caliper_config(args);

    ConfigManager mgr;

    mgr.add_config_spec(progress_config_spec);

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
            print_caliquery_help(args, usage, mgr);
            return 0;
        }

        if (args.is_set("version")) {
            cerr << cali_caliper_version() << std::endl;
            return 0;
        }
    }

    bool verbose = args.is_set("verbose");

    mgr.add(args.get("caliper-config").c_str());

    if (mgr.error()) {
        std::cerr << "cali-query: Caliper config parse error: "
                  << mgr.error_msg() << std::endl;
        return -1;
    }

    mgr.start();

    cali_set_global_string_byname("cali-query.build.date", __DATE__);
    cali_set_global_string_byname("cali-query.build.time", __TIME__);
#ifdef __GNUC__
    cali_set_global_string_byname("cali-query.build.compiler", "gnu-" __VERSION__);
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

    QueryArgsParser   query_parser;

    if (!query_parser.parse_args(args)) {
        cerr << "cali-query: Invalid query: " << query_parser.error_msg() << std::endl;
        return -2;
    }

    QuerySpec         spec = query_parser.spec();

    // setup format spec

    FormatProcessor   format(spec, stream);

    NodeProcessFn     node_proc = [](CaliperMetadataAccessInterface&,const Node*) { return; };
    SnapshotProcessFn snap_proc = [](CaliperMetadataAccessInterface&,const EntryList&){ return; };

    Aggregator        aggregate(spec);

    if (!args.is_set("list-globals")) {
        if (spec.aggregation_ops.selection == QuerySpec::AggregationSelection::None)
            snap_proc = format;
        else
            snap_proc = aggregate;

        if (!spec.preprocess_ops.empty())
            snap_proc = SnapshotFilterStep(Preprocessor(spec),   snap_proc);
        if (spec.filter.selection == QuerySpec::FilterSelection::List)
            snap_proc = SnapshotFilterStep(RecordSelector(spec), snap_proc);

        if (args.is_set("list-attributes")) {
            node_proc = AttributeExtract(snap_proc);
            snap_proc = [](CaliperMetadataAccessInterface&,const EntryList&){ return; };
        }
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

    cali_set_global_int_byname("cali-query.num-threads", num_threads);

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
            g_t(Annotation("thread", CALI_ATTR_SCOPE_THREAD).begin(static_cast<int>(t)));

        for (unsigned i = index++; i < files.size(); i = index++) { // "index++" is atomic read-mod-write
            const char* filename = (files[i].empty() ? "stdin" : files[i].c_str());

            Annotation::Guard
                g_s(Annotation("cali-query.stream", CALI_ATTR_SCOPE_THREAD).begin(filename));

            if (verbose) {
                std::lock_guard<std::mutex>
                    g(msgmutex);

                std::cerr << "cali-query: Reading " << filename << std::endl;
            }

            CaliReader reader(files[i]);

            if (!reader.read(metadb, node_proc, snap_proc)) {
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

    if (args.is_set("list-globals")) {
        if (spec.attribute_selection.selection != QuerySpec::AttributeSelection::List) {
            //   Global attributes will not be printed by default.
            // If the user didn't provide a selection, add all global attributes
            // to the selection list explictly.

            spec.attribute_selection.selection = QuerySpec::AttributeSelection::List;

            for (const Attribute& attr : metadb.get_all_attributes())
                if (attr.is_global())
                    spec.attribute_selection.list.push_back(attr.name());
        }

        FormatProcessor global_format(spec, stream);

        global_format.process_record(metadb, metadb.get_globals());
        global_format.flush(metadb);
    } else {
        aggregate.flush(metadb, format);
        format.flush(metadb);
    }

    CALI_MARK_END("Writing");

    mgr.flush();
}

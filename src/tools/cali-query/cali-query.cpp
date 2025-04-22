// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// A basic tool for Caliper metadata queries

#include "caliper/caliper-config.h"

#include "AttributeExtract.h"
#include "query_common.h"

#include "../util/Args.h"

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

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

using namespace cali;
using namespace util;

namespace
{
const char* usage =
    "cali-query [OPTION]... [FILE]..."
    "\n  Read, merge, and filter caliper streams";

const Args::Table option_table[] = {
    // name, longopt name, shortopt char, has argument, info, argument info
    { "select",
      "select",
      's',
      true,
      "Filter records by selected attributes: [-]attribute[(<|>|=)value][:...]",
      "QUERY_STRING" },
    { "aggregate",
      "aggregate",
      0,
      true,
      "Aggregate snapshots using the given aggregation operators: (sum(attribute)|count)[:...]",
      "AGGREGATION_OPS" },
    { "aggregate-key",
      "aggregate-key",
      0,
      true,
      "List of attributes to aggregate over (collapses all other attributes): attribute[:...]",
      "ATTRIBUTES" },
    { "expand", "expand", 'e', false, "Print records as comma-separated key=value lists", nullptr },
    { "attributes",
      "print-attributes",
      0,
      true,
      "Select attributes to print (or hide) in expanded output: [-]attribute[:...]",
      "ATTRIBUTES" },
    { "sort", "sort-by", 'S', true, "Sort rows in table format: attribute[:...]", "SORT_ATTRIBUTES" },
    { "format",
      "format",
      'f',
      true,
      "Format output according to format string: %[<width+alignment(l|r|c)>]attr_name%...",
      "FORMAT_STRING" },
    { "title", "title", 0, true, "Set the title row for formatted output", "STRING" },
    { "table", "table", 't', false, "Print records in human-readable table form", nullptr },
    { "tree",
      "tree",
      'T',
      false,
      "Print records in a tree based on the hierarchy of the selected path attributes",
      nullptr },
    { "path-attributes", "path-attributes", 0, true, "Select the path attributes for tree printers", "ATTRIBUTES" },
    { "json", "json", 'j', false, "Print given attributes in web-friendly json format", "ATTRIBUTES" },
    { "query", "query", 'q', true, "Execute a query in CalQL format", "QUERY STRING" },
    { "query-file", "query-file", 'Q', true, "Read a CalQL query from a file", "FILENAME" },
    { "caliper-config",
      "caliper-config",
      'P',
      true,
      "Set Caliper configuration for profiling cali-query",
      "CALIPER-CONFIG" },
    { "verbose", "verbose", 'v', false, "Be verbose.", nullptr },
    { "version", "version", 'V', false, "Print version number", nullptr },
    { "output", "output", 'o', true, "Set the output file name", "FILE" },
    { "help", "help", 'h', true, "Print help message", nullptr },
    { "list-attributes",
      "list-attributes",
      0,
      false,
      "List attribute info. Use with -j, -t, etc. to select output format.",
      nullptr },
    { "list-globals",
      "list-globals",
      'G',
      false,
      "List global run metadata. Use with -j, -t, etc. to select output format.",
      nullptr },
    Args::Terminator
};

/// A node record filter that filters redundant identical node records.
/// Redundant node records can occur when merging/unifying two streams.
class FilterDuplicateNodes
{
    cali_id_t m_max_node;

public:

    FilterDuplicateNodes() : m_max_node { 0 } {}

    void operator() (CaliperMetadataAccessInterface& db, const Node* node, NodeProcessFn push)
    {
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

    NodeFilterStep(NodeFilterFn filter_fn, NodeProcessFn push_fn) : m_filter_fn { filter_fn }, m_push_fn { push_fn } {}

    void operator() (CaliperMetadataAccessInterface& db, const Node* node) { m_filter_fn(db, node, m_push_fn); }
};

} // namespace

const char* progress_config_spec =
    "{"
    " \"name\"        : \"caliquery-progress\","
    " \"description\" : \"Print cali-query progress (when processing multiple files)\","
    " \"services\"    : [ \"event\", \"textlog\", \"timer\" ],"
    " \"config\"      : {"
    "   \"CALI_CHANNEL_FLUSH_ON_EXIT\" : \"false\","
    "   \"CALI_EVENT_TRIGGER\"         : \"cali-query.stream\","
    "   \"CALI_TEXTLOG_TRIGGER\"       : \"cali-query.stream\","
    "   \"CALI_TEXTLOG_FILENAME\"      : \"stderr\","
    "   \"CALI_TEXTLOG_FORMATSTRING\"  : "
    "     \"cali-query: Processed %[52]cali-query.stream% %[6]time.duration.ns% ns\""
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
            std::cerr << "cali-query: error: unknown option: " << argv[i] << '\n' << "  Available options: ";
            args.print_available_options(std::cerr);
            return -1;
        }

        if (args.is_set("help")) {
            print_caliquery_help(args, usage, mgr);
            return 0;
        }

        if (args.is_set("version")) {
            std::cerr << cali_caliper_version() << std::endl;
            return 0;
        }
    }

    bool verbose = args.is_set("verbose");

    mgr.add(args.get("caliper-config").c_str());

    if (mgr.error()) {
        std::cerr << "cali-query: Caliper config parse error: " << mgr.error_msg() << std::endl;
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

    QueryArgsParser query_parser;

    if (!query_parser.parse_args(args)) {
        std::cerr << "cali-query: Invalid query: " << query_parser.error_msg() << std::endl;
        return -2;
    }

    QuerySpec spec = query_parser.spec();

    // setup format spec

    FormatProcessor format(spec, stream);

    NodeProcessFn node_proc = [](CaliperMetadataAccessInterface&, const Node*) {
        return;
    };
    SnapshotProcessFn snap_proc = [](CaliperMetadataAccessInterface&, const EntryList&) {
        return;
    };

    Aggregator aggregate(spec);

    if (!args.is_set("list-globals")) {
        if (spec.aggregate.selection == QuerySpec::AggregationSelection::None)
            snap_proc = format;
        else
            snap_proc = aggregate;

        if (spec.filter.selection == QuerySpec::FilterSelection::List)
            snap_proc = SnapshotFilterStep(RecordSelector(spec), snap_proc);
        if (!spec.preprocess_ops.empty())
            snap_proc = SnapshotFilterStep(Preprocessor(spec), snap_proc);

        if (args.is_set("list-attributes")) {
            node_proc = AttributeExtract(snap_proc);
            snap_proc = [](CaliperMetadataAccessInterface&, const EntryList&) {
                return;
            };
        }
    }

    node_proc = ::NodeFilterStep(::FilterDuplicateNodes(), node_proc);

    std::vector<std::string> files = args.arguments();

    if (files.empty())
        files.push_back(""); // read from stdin if no files are given

    CALI_MARK_END("Initialization");

    //
    // --- Process files
    //

    CALI_MARK_BEGIN("Processing");

    CaliperMetadataDB metadb;

    metadb.add_attribute_aliases(spec.aliases);
    metadb.add_attribute_units(spec.units);

    for (const std::string& file : files) {
        Annotation::Guard g_f(Annotation("cali-query.stream").begin(file.empty() ? "stdin" : file.c_str()));

        if (verbose)
            std::cerr << "cali-query: Reading " << file << std::endl;

        CaliReader reader;
        reader.read(file, metadb, node_proc, snap_proc);

        if (reader.error())
            std::cerr << "cali-query: Error reading " << file << ": " << reader.error_msg() << std::endl;
    }

    CALI_MARK_END("Processing");

    //
    // --- Flush outputs
    //

    CALI_MARK_BEGIN("Writing");

    if (args.is_set("list-globals")) {
        if (spec.select.selection != QuerySpec::AttributeSelection::List) {
            //   Global attributes will not be printed by default.
            // If the user didn't provide a selection, add all global attributes
            // to the selection list explictly.

            spec.select.selection = QuerySpec::AttributeSelection::List;

            for (const Attribute& attr : metadb.get_all_attributes())
                if (attr.is_global())
                    spec.select.list.push_back(attr.name());
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

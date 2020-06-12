// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "query_common.h"

#include "caliper/cali-mpi.h"
#include "caliper/cali.h"
#include "caliper/cali-manager.h"

#include "caliper/tools-util/Args.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CaliReader.h"
#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/Preprocessor.h"
#include "caliper/reader/QuerySpec.h"
#include "caliper/reader/RecordProcessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/StringConverter.h"

#include <iostream>

using namespace cali;
using namespace util;

namespace
{

const char* usage = "mpi-caliquery [OPTION]... "
    "\n  Read, merge, and filter caliper streams in parallel."
    "\n  Reads data from <rank>.cali on each MPI rank (i.e., 0.cali, 1.cali, ...)";

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
    { "query", "query", 'q', true,
      "Execute a query in CalQL format",
      "QUERY STRING"
    },
    { "caliper-config", "caliper-config", 'P', true,
      "Set Caliper configuration for profiling mpi-caliquery",
      "CALIPER-CONFIG"
    },
    { "caliper-config-vars", "caliper-config-vars", 0, true,
      "Caliper configuration flags (for cali-query profiling)",
      "KEY=VALUE,..."
    },
    { "verbose", "verbose", 'v', false, "Be verbose.",              nullptr },
    { "help",    "help",    'h', true,  "Print help message",       nullptr },
    { "output", "output",   'o', true,  "Set the output file name", "FILE"  },
    Args::Table::Terminator
};


void format_output(const Args& args, const QuerySpec& spec, CaliperMetadataAccessInterface& db, Aggregator& aggregate)
{
    CALI_CXX_MARK_FUNCTION;

    OutputStream stream;

    if (args.is_set("output"))
        stream.set_filename(args.get("output").c_str());
    else
        stream.set_stream(OutputStream::StdOut);

    FormatProcessor format(spec, stream);

    aggregate.flush(db, format);
    format.flush(db);
}

void process_my_input(int rank, const Args& args, const QuerySpec& spec, CaliperMetadataDB& db, Aggregator& aggregate)
{
    CALI_CXX_MARK_FUNCTION;

    std::string filename = std::to_string(rank) + ".cali";

    if (!args.arguments().empty())
        if (!args.arguments().front().empty())
            filename = args.arguments().front() + "/" + filename;

    CaliReader reader(filename);

    NodeProcessFn     node_proc = [](CaliperMetadataAccessInterface&,const Node*) { return; };
    SnapshotProcessFn snap_proc = aggregate;

    if (!spec.preprocess_ops.empty())
        snap_proc = SnapshotFilterStep(Preprocessor(spec),   snap_proc);
    if (spec.filter.selection == QuerySpec::FilterSelection::List)
        snap_proc = SnapshotFilterStep(RecordSelector(spec), snap_proc);

    if (!reader.read(db, node_proc, snap_proc))
        std::cerr << "mpi-caliquery (" << rank << "): cannot read " << filename << std::endl;
}

void setup_caliper_config(const Args& args)
{
    cali_config_preset("CALI_LOG_VERBOSITY", "0");
    cali_config_preset("CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE", "process");

    cali_config_allow_read_env(false);

    cali_config_set("CALI_CONFIG_FILE", "mpi-caliquery_caliper.config");

    if (args.is_set("verbose"))
        cali_config_preset("CALI_LOG_VERBOSITY", "2");

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

} // namespace [anonymous]


int main(int argc, char* argv[])
{
    cali_mpi_init();

    // --- Parse command line arguments
    //

    Args args(::option_table);
    int first_unknown_arg = args.parse(argc, argv);

    // must be done before Caliper initialization in MPI_Init wrapper
    ::setup_caliper_config(args);
    cali::ConfigManager mgr;

    MPI_Init(&argc, &argv);

    int rank;
    int worldsize;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldsize);

    if (first_unknown_arg < argc) {
        if (rank == 0) {
            std::cerr << "mpi-caliquery: error: unknown option: " << argv[first_unknown_arg] << '\n'
                      << "  Available options: ";

            args.print_available_options(std::cerr);
        }

        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    QueryArgsParser query_parser;

    if (!query_parser.parse_args(args)) {
        if (rank == 0)
            std::cerr << "mpi-caliquery: Invalid query: " << query_parser.error_msg()
                      << std::endl;

        MPI_Abort(MPI_COMM_WORLD, -2);
    }

    if (args.is_set("help")) {
        if (rank == 0)
            print_caliquery_help(args, usage, mgr);

        MPI_Finalize();
        return 0;
    }

    mgr.add(args.get("caliper-config").c_str());

    if (mgr.error())
        if (rank == 0)
            std::cerr << "mpi-caliquery: Caliper config parse error: " << mgr.error_msg() << std::endl;

    mgr.start();

    CALI_MARK_FUNCTION_BEGIN;

    QuerySpec  spec = query_parser.spec();

    Aggregator aggregate(spec);
    CaliperMetadataDB metadb;

    // --- Process our own input
    //

    ::process_my_input(rank, args, spec, metadb, aggregate);

    // --- Aggregation loop
    //

    aggregate_over_mpi(metadb, aggregate, MPI_COMM_WORLD);

    // --- Print output
    //

    if (rank == 0)
        ::format_output(args, spec, metadb, aggregate);

    CALI_MARK_FUNCTION_END;

    mgr.flush();

    MPI_Finalize();

    return 0;
}

// Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
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

#include "query_common.h"

#include "caliper/cali-mpi.h"
#include "caliper/cali.h"

#include "caliper/tools-util/Args.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/QuerySpec.h"
#include "caliper/reader/RecordProcessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/StringConverter.h"
#include "caliper/common/csv/CsvReader.h"
#include "caliper/common/csv/CsvWriter.h"

#include <iostream>

using namespace cali;
using namespace util;

namespace
{
    
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
    { "profile", "profile", 'p', false,
      "Show progress and cali-query performance summary",
      nullptr
    },
    { "caliper-config", "caliper-config", 0, true,
      "Caliper configuration flags (for cali-query profiling)",
      "KEY=VALUE,..."
    },
    { "verbose", "verbose", 'v', false,
      "Be verbose.",
      nullptr
    },
    { "output", "output", 'o', true,  "Set the output file name", "FILE"  },
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

    CsvReader reader(filename);
    IdMap idmap;

    NodeProcessFn     node_proc = [](CaliperMetadataAccessInterface&,const Node*) { return; };
    SnapshotProcessFn snap_proc = aggregate;

    if (spec.filter.selection == QuerySpec::FilterSelection::List)
        snap_proc = SnapshotFilterStep(RecordSelector(spec), snap_proc);

    if (!reader.read([&](const RecordMap& rec){ db.merge(rec, idmap, node_proc, snap_proc); }))
        std::cerr << "mpi-caliquery (" << rank << "): cannot read " << filename << std::endl;
}

void setup_caliper_config(const Args& args)
{
    const char* summary_profile[][2] = {
        { "CALI_SERVICES_ENABLE", "aggregate:event:mpi:mpireport:textlog:timestamp" },
        { "CALI_AGGREGATE_KEY", "function" },
        { "CALI_EVENT_TRIGGER", "function" },

        { "CALI_MPIREPORT_CONFIG",
          "SELECT function,statistics(sum#time.inclusive.duration) GROUP BY function FORMAT table" },

        { NULL, NULL }
    };
    
    cali_config_preset("CALI_LOG_VERBOSITY", "0");
    cali_config_preset("CALI_CALIPER_ATTRIBUTE_PROPERTIES", "annotation=process_scope:nested");

    cali_config_allow_read_env(false);

    cali_config_define_profile("mpi-caliquery_summary_profile", summary_profile);

    cali_config_set("CALI_CONFIG_FILE", "mpi-caliquery_caliper.config");
    
    if (args.is_set("verbose"))
        cali_config_preset("CALI_LOG_VERBOSITY", "1");
    if (args.is_set("profile"))
        cali_config_set("CALI_CONFIG_PROFILE", "mpi-caliquery_summary_profile");

    std::vector<std::string> config_list = 
        StringConverter(args.get("caliper-config")).to_stringlist();

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
    // --- Parse command line arguments
    //

    Args args(::option_table);
    int first_unknown_arg = args.parse(argc, argv);

    // must be done before Caliper initialization in MPI_Init wrapper
    ::setup_caliper_config(args);
    
    MPI_Init(&argc, &argv);

    int rank;
    int worldsize;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldsize);
    
    CALI_CXX_MARK_FUNCTION;
    
    if (first_unknown_arg < argc) {
        if (rank == 0) {
            std::cerr << "mpi-caliquery: error: unknown option: " << argv[first_unknown_arg] << '\n'
                      << "  Available options: ";
                
            args.print_available_options(std::cerr);
        }
        
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    QuerySpec  spec = spec_from_args(args);
    
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

    MPI_Finalize();

    return 0;
}

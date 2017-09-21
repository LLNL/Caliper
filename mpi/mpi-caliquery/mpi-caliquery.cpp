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

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"
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

void process_my_input(int rank, const Args& args, CaliperMetadataDB& db, Aggregator& aggregate)
{
    CALI_CXX_MARK_FUNCTION;

    std::string filename = std::to_string(rank) + ".cali";

    if (!args.arguments().empty())
        if (!args.arguments().front().empty())
            filename = args.arguments().front() + "/" + filename;

    CsvReader reader(filename);
    IdMap idmap;

    NodeProcessFn nodeproc = [](CaliperMetadataAccessInterface&,const Node*) { return; };

    if (!reader.read([&](const RecordMap& rec){ db.merge(rec, idmap, nodeproc, aggregate); }))
        std::cerr << "mpi-caliquery (" << rank << "): cannot read " << filename << std::endl;
}

} // namespace [anonymous]


int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    int rank;
    int worldsize;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldsize);
    
    CALI_CXX_MARK_FUNCTION;
    
    // --- Parse command line arguments
    //

    Args args(::option_table);

    {
        int i;

        if ((i = args.parse(argc, argv)) < argc) {
            if (rank == 0) {
                std::cerr << "cali-query: error: unknown option: " << argv[i] << '\n'
                          << "  Available options: ";
                
                args.print_available_options(std::cerr);
            }
        
            MPI_Abort(MPI_COMM_WORLD, -1);
        }
    }

    QuerySpec  spec = spec_from_args(args);
    
    Aggregator aggregate(spec);
    CaliperMetadataDB metadb;
    
    // --- Process our own input
    //

    ::process_my_input(rank, args, metadb, aggregate);

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

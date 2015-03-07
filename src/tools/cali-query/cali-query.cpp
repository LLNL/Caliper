/// @file cali-query.cpp
/// A basic tool for Caliper metadata queries

#include "Args.h"
#include "RecordProcessor.h"
#include "RecordSelector.h"

#include <CaliperMetadataDB.h>

#include <CsvReader.h>

#include <ContextRecord.h>
#include <Node.h>

#include <util/split.hpp>

#include <iostream>
#include <iterator>

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
          "Select context records: [-]attribute[(<|>|=)value][:...]", "QUERY_STRING" 
        },
        { "expand", "expand", 'e', false, "Expand context records",   nullptr },
        { "output", "output", 'o', true,  "Set the output file name", "FILE"  },
        { "help",   "help",   'h', false, "Print help message",       nullptr },
        Args::Table::Terminator
    };

    void write_keyval(CaliperMetadataDB& db, const RecordMap& rec) {
        int nentry = 0;

        for (auto const &entry : ContextRecord::unpack(rec, std::bind(&CaliperMetadataDB::node, &db, std::placeholders::_1)))
            if (!entry.second.empty()) {
                cout << (nentry++ ? "," : "") << entry.first << "=";

                int nelem = 0;
                for (const Variant &elem : entry.second)
                    cout << (nelem++ ? "/" : "") << elem.to_string();
            }

        if (nentry > 0)
            cout << endl;
    }

    void write_record(CaliperMetadataDB& /* cb */, const RecordMap& rec) {
        cout << rec << endl;
    }


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
    Args args(::option_table);


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
            cerr << usage << "\n\n";

            args.print_available_options(cerr);

            return 0;
        }
    }


    //
    // --- Build up processing chain (from back to front)
    //

    RecordProcessFn   processor = [](CaliperMetadataDB&,const RecordMap&){ return; };

    if (args.is_set("expand"))
        processor = ::write_keyval;
    else 
        processor = ::write_record;

    string select = args.get("select");

    if (!select.empty())
        processor = ::FilterStep(RecordSelector(select), processor);

    processor = ::FilterStep(::FilterDuplicateNodes(), processor);


    //
    // --- Process inputs
    //

    CaliperMetadataDB metadb;

    for (const string& file : args.arguments()) {
        CsvReader reader(file);
        IdMap     idmap;

        if (!reader.read([&](const RecordMap& rec){ processor(metadb, metadb.merge(rec, idmap)); }))
            cerr << "Could not read file " << file << endl;
    }
}

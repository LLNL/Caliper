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
    const char* usage = "Usage: cali-query <data file 1> ... <data file n>";

    const Args::Table argtbl[] = { 
        { "select", "select=QUERY STRING", 's', true,  "Select context records: [-]attribute[(<|>|=)value][,...]" },
        { "format", "format=(csv|expand)", 'f', true,  "Set the output format"    },
        { "output", "output=FILE",         'o', true,  "Set the output file name" },
        { "help",   "help",                'h', false, "Print help message"       },

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

    struct FilterStep {
        RecordFilterFn  m_filter_fn;
        RecordProcessFn m_push_fn;

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
    Args args(::argtbl);

    {
        int i = args.parse(argc, argv);

        if (i < argc) {
            cerr << "cali-query: error: unknown option: " << argv[i] << endl;
            return -1;
        }
    }

    if (args.is_set("help")) {
        cerr << "cali-query [OPTION]... [FILE]...\n" << endl;
        args.print_available_options(cerr);
        return 0;
    }

    for (auto opt : args.options())
        cerr << opt << endl;
    for (auto arg : args.arguments())
        cerr << arg << endl;

    return 0;

    RecordProcessFn   processor = [](CaliperMetadataDB&,const RecordMap&){ return; };    

    // Build up processing chain back-to-front

    string format = args.get("format", "csv");

    if (format == "csv")
        processor = ::write_record;
    else if (format == "expanded")
        processor = ::write_keyval;
    else 
        cerr << "Unknown output format: " << format << endl;

    string select = args.get("select");

    if (!select.empty())
        processor = ::FilterStep(RecordSelector(select), processor);

    processor = ::FilterStep(::FilterDuplicateNodes(), processor);

    CaliperMetadataDB metadb;

    for (const string& file : args.arguments()) {
        CsvReader reader(file);
        IdMap     idmap;

        if (!reader.read([&](const RecordMap& rec){ processor(metadb, metadb.merge(rec, idmap)); }))
            cerr << "Could not read file " << file << endl;
    }
}

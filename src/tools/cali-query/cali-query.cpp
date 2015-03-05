/// @file cali-query.cpp
/// A basic tool for Caliper metadata queries

#include "RecordProcessor.h"

#include <CaliperMetadataDB.h>

#include <CsvReader.h>

#include <ContextRecord.h>
#include <Node.h>

#include <util/split.hpp>

#include <iostream>
#include <iterator>

using namespace cali;
using namespace std;

namespace
{
    const char* usage = "Usage: cali-query <data file 1> ... <data file n>";

    vector<uint64_t> read_context_string(const string& contextstring) {
        vector<string>   strs;

        util::split(contextstring, ':', back_inserter(strs));

        vector<uint64_t> ctx;

        for (const string &s : strs)
            ctx.push_back(stoul(s));

        return ctx;
    }


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
            auto rec_entry_it = rec.find("__rec");

            if (rec_entry_it != rec.end() && !rec_entry_it->second.empty() && rec_entry_it->second.front().to_string() == "node") {
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

int main(int argc, char* argv[])
{
    if (argc < 2) {
        cerr << ::usage << endl;
        return -1;
    }

    RecordProcessFn   processor = [](CaliperMetadataDB&,const RecordMap&){ return; };    

    processor = ::write_record;
    processor = ::FilterStep(::FilterDuplicateNodes(), processor);

    CaliperMetadataDB metadb;

    for (int i = 1; i < argc; ++i) {
        CsvReader reader(argv[i]);
        IdMap     idmap;

        if (!reader.read([&](const RecordMap& rec){ processor(metadb, metadb.merge(rec, idmap)); }))
            cerr << "Could not read file " << argv[i] << endl;
    }
}

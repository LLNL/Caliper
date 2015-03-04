/// @file cali-query.cpp
/// A basic tool for Caliper metadata queries

#include <CaliperMetadataDB.h>

#include <CsvReader.h>

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

    CaliperMetadataDB metadb;

    for (int i = 1; i < argc; ++i) {
        CsvReader reader(argv[i]);
        IdMap     idmap;

        if (!reader.read([&](const RecordMap& rec){ cout << metadb.merge(rec, idmap) << endl; }))
            cerr << "Could not read file " << argv[i] << endl;
    }
}

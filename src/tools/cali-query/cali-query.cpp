/// @file cali-query.cpp
/// A basic tool for Caliper metadata queries

#include "CaliperMetadataDB.h"

#include <Node.h>

#include <util/split.hpp>

#include <iostream>
#include <iterator>

using namespace cali;
using namespace std;

namespace
{
    const char* usage = "Usage: cali-query <metadata file> <context record>";

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
    if (argc < 3) {
        cerr << ::usage << endl;
        return -1;
    }

    CaliperMetadataDB metadb;

    if (!metadb.read(argv[1])) {
        cerr << "Error: could not read metadata file \"" << argv[1] << "\"" << endl;
        return -2;
    }

    vector<uint64_t> ctx = ::read_context_string(argv[2]);        
}

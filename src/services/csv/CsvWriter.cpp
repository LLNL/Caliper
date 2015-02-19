/// @file CsvWriter.cpp
/// CsvWriter implementation


#include "CsvWriter.h"

#include "CsvSpec.h"

#include <Attribute.h>
#include <Log.h>
#include <Node.h>
#include <RuntimeConfig.h>

#include <fstream>
#include <iostream>

using namespace cali;
using namespace std;

namespace
{

const ConfigSet::Entry csv_configdata[] = {
    { "basename", CALI_TYPE_STRING, "caliper",
      "Base filename for .nodes.csv files",
      "Base filename for .nodes.csv files"
    },
    ConfigSet::Terminator
};

}


struct CsvWriter::CsvWriterImpl
{
    std::string node_file;
    ConfigSet   m_config;

    CsvWriterImpl()
        : m_config { RuntimeConfig::init("csv", ::csv_configdata) } 
    {
        node_file = m_config.get("basename").to_string() + ".nodes.csv";
    }

    CsvWriterImpl(const string& basename)
        : m_config { RuntimeConfig::init("csv", ::csv_configdata) } 
    {
        node_file = basename + ".nodes.csv";
    }
};

//
// -- public interface
//    

CsvWriter::CsvWriter()
    : mP { new CsvWriterImpl }
{ }

CsvWriter::CsvWriter(const std::string& basename)
    : mP { new CsvWriterImpl(basename) }
{ }

CsvWriter::~CsvWriter()
{
    mP.reset();
}

bool CsvWriter::write(std::function<void(std::function<void(const Node&)>)> foreach_node)
{
    if (mP->node_file.empty()) {
        cout << "Nodes:" << endl;
        foreach_node([](const Node&  n){ CsvSpec::write_record(cout, n.rec()); });
    } else {
        ofstream nstr(mP->node_file.c_str());

        if (!nstr)
            return false;

        foreach_node([&](const Node& n){ CsvSpec::write_record(nstr, n.rec()); });

        Log(1).stream() << "Wrote " << mP->node_file << endl;
    }

    return true;
}

namespace cali
{
    void csv_writer_register() {
        RuntimeConfig::init("csv", ::csv_configdata);

        Log(2).stream() << "Registered csv writer" << endl;
    }

    std::unique_ptr<MetadataWriter> csv_writer_create() {
        return unique_ptr<MetadataWriter>( new CsvWriter() );
    }
}

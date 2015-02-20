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
    { "filename", CALI_TYPE_STRING, "caliper.csv",
      "CSV output file name",
      "CSV output file name"
    },
    ConfigSet::Terminator
};

}


struct CsvWriter::CsvWriterImpl
{
    ConfigSet m_config;

    CsvWriterImpl()
        : m_config { RuntimeConfig::init("csv", ::csv_configdata) } 
    { }

    CsvWriterImpl(const string& filename)
        : m_config { RuntimeConfig::init("csv", ::csv_configdata) }
    { }
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
    string   filename = mP->m_config.get("filename").to_string();
    ofstream nstr(filename.c_str());

    if (!nstr)
        return false;

    foreach_node([&](const Node& n){ 
            n.push_record([&](const RecordDescriptor r, const int* c, const Variant** data){
                    CsvSpec::write_record(nstr, r, c, data); });
                });

    Log(1).stream() << "Wrote " << filename << endl;

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

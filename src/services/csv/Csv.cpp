/// @file Csv.cpp
/// Csv implementation

#include "Csv.h"

#include <Attribute.h>
#include <Log.h>
#include <Node.h>
#include <RecordMap.h>
#include <RuntimeConfig.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <map>

using namespace cali;
using namespace std;

namespace 
{

struct CsvSpec
{


    std::string m_sep       { ","    }; ///< separator character
    char        m_esc       { '\\'   }; ///< escape character
    std::string m_esc_chars { "\\,=" }; ///< characters that need to be escaped

    // --- write interface

    void write_string(ostream& os, const char* str, size_t size) const {
        // os << '"';

        for (size_t i = 0; i < size; ++i) {
            if (m_esc_chars.find(str[i]) != string::npos)
                os << m_esc;
            os << str[i];
        }

        // os << '"';
    }

    void write_string(ostream& os, const string& str) const {
        write_string(os, str.data(), str.size());
    }


    // --- read interface

    vector<string> split(const string& list, char sep) const {
        vector<string> vec;
        string str;

        for (auto it = list.begin(); it != list.end(); ++it) {
            if (*it == sep) {
                vec.push_back(str);
                str.clear();
            } else if (*it == '"') {
                // read string
                for (++it; it != list.end() && *it != '"'; ++it)
                    if (*it == m_esc && (++it != list.end()))
                        str.push_back(*it);
            } else if (!isspace(*it))
                str.push_back(*it);
        }

        vec.push_back(str);

        return vec;
    }

    void write_record(ostream& os, const RecordMap& record) {
        int count = 0;

        for (auto &e : record) {
            os << (count++ ? m_sep : "") << e.first << '=';
            write_string(os, e.second.to_string());
        }

        if (count)
            os << endl;
    }

    RecordMap read_record(const string& line) {
        vector<string> entries = split(line, m_sep[0]);
        RecordMap      rec;

        for (const string& entry : entries) {
            vector<string> keyval = split(entry, '=');

            if (keyval.size() == 2)
                rec.insert(make_pair(keyval[0], Variant(keyval[1])));
            else
                Log(1).stream() << "Invalid CSV entry: " << entry << endl;
        }

        return rec;
    }
}; // struct CsvSpec

static CsvSpec CaliperCsvSpec;

} // anonymous namespace


//
// -- public interface
//    

struct CsvWriter::CsvWriterImpl
{
    static const ConfigSet::Entry s_configdata[];

    std::string node_file;
    ConfigSet   m_config;

    CsvWriterImpl()
        : m_config { RuntimeConfig::init("csv", ::CsvSpec::s_configdata) } 
    {
        node_file = m_config.get("basename").to_string() + ".nodes.csv";
    }
};

const ConfigSet::Entry CsvWriter::CsvWriterImpl::s_configdata[] = {
    { "basename", CALI_TYPE_STRING, "caliper",
      "Base filename for .nodes.csv files",
      "Base filename for .nodes.csv files"
    },
    ConfigSet::Terminator
};

CsvWriter::CsvWriter()
    : mP { new CsvWriterImpl }
{ }

CsvWriter::CsvWriter(const std::string& basename)
    : mP { new CsvWriterImpl }
{ }

CsvWriter::~CsvWriter()
{
    mP.reset();
}

bool CsvWriter::write(std::function<void(std::function<void(const Node&)>)> foreach_node)
{
    if (mP->node_file.empty()) {
        cout << "Nodes:" << endl;
        foreach_node([](const Node&  n){ CaliperCsvSpec.write_record(cout, n.record()); });
    } else {
        ofstream nstr(mP->node_file.c_str());

        if (!nstr)
            return false;

        foreach_node([&](const Node& n){ CaliperCsvSpec.write_record(nstr, n.record()); });

        Log(1).stream() << "Wrote " << mP->node_file << endl;
    }

    return true;
}

namespace cali
{
    void csv_writer_register() {
        RuntimeConfig::init("csv", ::CsvSpec::s_configdata);

        Log(2).stream() << "Registered csv writer" << endl;
    }

    std::unique_ptr<MetadataWriter> csv_writer_create() {
        return unique_ptr<MetadataWriter>( new CsvWriter() );
    }
}

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
    static const ConfigSet::Entry s_configdata[];

    std::string m_sep       { ","   }; ///< separator character
    std::string m_delim     { ":"   }; ///< delimiter
    char        m_esc       { '\\'  }; ///< escape character
    std::string m_esc_chars { "\\," }; ///< characters that need to be escaped

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

    std::vector<std::string> split(const std::string& list, char sep) const {
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

    vector<unsigned char> read_data(const std::string& str, cali_attr_type type) const {
        vector<unsigned char> data;

        switch (type) {
        case CALI_TYPE_USR:
            {
                // data is stored as sequence of hexadecimal byte values delimited by m_delim,
                // e.g. "42:0:0:2a:f:a0:2:0:"

                for (const string &s : split(str, m_delim[0]))
                    if (s.size())
                        data.push_back(static_cast<unsigned char>(std::stoul(s, 0, 16)));
            }
            break;

        case CALI_TYPE_INT:
            {
                union { int64_t i; unsigned char bytes[sizeof(int64_t)];   } u;
                u.i = std::stoi(str);
                std::copy(u.bytes, u.bytes + sizeof(int64_t),  std::back_inserter(data));
            }
            break;
        case CALI_TYPE_ADDR:
            {
                union { uint64_t i; unsigned char bytes[sizeof(uint64_t)]; } u;
                u.i = std::stoul(str, 0, 16); // read hexadecimal value
                std::copy(u.bytes, u.bytes + sizeof(uint64_t), std::back_inserter(data));
            }
            break;
        case CALI_TYPE_DOUBLE:
            {
                union { double d; unsigned char bytes[sizeof(double)];     } u;
                u.d = std::stod(str);
                std::copy(u.bytes, u.bytes + sizeof(double),   std::back_inserter(data));
            }
            break;

        case CALI_TYPE_STRING:
            std::copy(str.begin(), str.end(), std::back_inserter(data));
            break;
        }

        return data;
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
}; // struct CsvSpec

static CsvSpec CaliperCsvSpec;

const ConfigSet::Entry CsvSpec::s_configdata[] = {
    { "basename", CALI_TYPE_STRING, "caliper",
      "Base filename for .attributes.csv and .nodes.csv files",
      "Base filename for .attributes.csv and .nodes.csv files"
    },
    ConfigSet::Terminator
};

} // anonymous namespace


//
// -- public interface
//    

struct CsvWriter::CsvWriterImpl
{
    std::string node_file;
    ConfigSet   m_config;

    CsvWriterImpl()
        : m_config { RuntimeConfig::init("csv", ::CsvSpec::s_configdata) } 
    {
        node_file = m_config.get("basename").to_string() + ".nodes.csv";
    }
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

bool CsvWriter::write(std::function<void(std::function<void(const Node&)>)>      foreach_node)
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

/// @file CsvSpec.cpp
/// CsvSpec implementation

#include "CsvSpec.h"

#include <Record.h>
#include <Log.h>

#include <vector>

using namespace cali;
using namespace std;

namespace 
{

struct CsvSpecImpl
{
    static CsvSpecImpl s_caliper_csvspec;

    string m_sep       { ","    }; ///< separator character
    char   m_esc       { '\\'   }; ///< escape character
    string m_esc_chars { "\\,=" }; ///< characters that need to be escaped

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
            } else 
                str.push_back(*it);
        }

        vec.push_back(str);

        return vec;
    }

    void write_record(ostream& os, const RecordDescriptor& record, const int count[], const Variant* data[]) {
        os << "__rec=" << record.name;

        for (unsigned e = 0; e < record.num_entries; ++e) {
            if (count[e] > 0)
                os << "," << record.entries[e];
            for (int c = 0; c < count[e]; ++c)
                os << "=" << data[e][c].to_string();
        }

        os << endl;
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
};

CsvSpecImpl CsvSpecImpl::s_caliper_csvspec;

} // namespace

void
CsvSpec::write_record(ostream& os, const RecordMap& record)
{
    ::CsvSpecImpl::s_caliper_csvspec.write_record(os, record);
}

void
CsvSpec::write_record(ostream& os, const RecordDescriptor& record, const int* data_count, const Variant** data)
{
    ::CsvSpecImpl::s_caliper_csvspec.write_record(os, record, data_count, data);
}

RecordMap 
CsvSpec::read_record(const string& line)
{
    return ::CsvSpecImpl::s_caliper_csvspec.read_record(line);
}

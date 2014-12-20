/// @file CsvSpec.cpp
/// CsvSpec implementation

#include "CsvSpec.h"

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
};

CsvSpecImpl CsvSpecImpl::s_caliper_csvspec;

} // namespace

void
CsvSpec::write_record(ostream& os, const RecordMap& record)
{
    ::CsvSpecImpl::s_caliper_csvspec.write_record(os, record);
}

RecordMap 
CsvSpec::read_record(const string& line)
{
    return ::CsvSpecImpl::s_caliper_csvspec.read_record(line);
}

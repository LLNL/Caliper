/// @file Expand.cpp
/// Print expanded records

#include "Expand.h"

#include <ContextRecord.h>
#include <CaliperMetadataDB.h>

#include <util/split.hpp>

#include <functional>
#include <iterator>
#include <set>


using namespace cali;
using namespace std;

struct Expand::ExpandImpl
{
    set<string> m_selected;
    set<string> m_deselected;

    ostream&    m_os;

    ExpandImpl(ostream& os)
        : m_os { os }
        { }

    void parse(const string& field_string) {
        vector<string> fields;

        util::split(field_string, ':', back_inserter(fields));

        for (const string& s : fields) {
            if (s.size() == 0)
                continue;
            
            string::size_type spos   = 0;
            bool              negate = false;

            if (s[0] == '-')
                m_deselected.emplace(s, 1, string::npos);
            else
                m_selected.insert(s);
        }
    }

    void print(CaliperMetadataDB& db, const RecordMap& rec) {
        int nentry = 0;

        for (auto const &entry : ContextRecord::unpack(rec, std::bind(&CaliperMetadataDB::node, &db, std::placeholders::_1))) {
            if (entry.second.empty())
                continue;
            if ((!m_selected.empty() && m_selected.count(entry.first) == 0) || m_deselected.count(entry.first))
                continue;

            m_os << (nentry++ ? "," : "") << entry.first << "=";

            int nelem = 0;
            for (const Variant &elem : entry.second)
                m_os << (nelem++ ? "/" : "") << elem.to_string();
        }

        if (nentry > 0)
            m_os << endl;
    }
};


Expand::Expand(ostream& os, const string& field_string)
    : mP { new ExpandImpl(os) }
{
    mP->parse(field_string);
}

Expand::~Expand()
{
    mP.reset();
}

void
Expand::operator()(CaliperMetadataDB& db, const RecordMap& rec) const
{
    mP->print(db, rec);
}

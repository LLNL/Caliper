// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Print expanded records

#include "caliper/reader/UserFormatter.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"

#include "caliper/common/util/split.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <mutex>
#include <set>
#include <sstream>

using namespace cali;
using namespace std;

struct UserFormatter::FormatImpl
{
    struct Field {
        string    prefix;
        
        string    attr_name;
        Attribute attr;

        size_t    width;
        char      align; // 'l', 'r', 'c'
    }; // for the frmtparse function

    vector<Field> m_fields; // for the format strings
    string        m_title;

    std::mutex    m_fields_lock;

    OutputStream  m_os;
    std::mutex    m_os_lock;
    
    FormatImpl(OutputStream& os)
        : m_os(os)
        { }

    // based on SnapshotTextFormatter::parse
    void parse(const string& formatstring) {

        // parse format: "(prefix string)%[<width+alignment(l|r|c)>]attr_name%..."
        vector<string> split_string;

        util::split(formatstring, '%', back_inserter(split_string));

        while (!split_string.empty()) {
            Field field = { "", "", Attribute::invalid, 0, 'l' };

            field.prefix = split_string.front();
            split_string.erase(split_string.begin());

            // parse field entry
            if (!split_string.empty()) {
                vector<string> field_strings;
                util::tokenize(split_string.front(), "[]", back_inserter(field_strings));

                // look for width/alignment specification (in [] brackets)

                int wfbegin = -1;
                int wfend   = -1;
                int apos    = -1;

                int nfields = field_strings.size();

                for (int i = 0; i < nfields; ++i)
                    if(field_strings[i] == "[")
                        wfbegin = i;
                    else if (field_strings[i] == "]")
                        wfend = i;

                if (wfbegin >= 0 && wfend > wfbegin+1) {
                    // width field specified
                    field.width = stoi(field_strings[wfbegin+1]);

                    if (wfbegin > 0)
                        apos = 0;
                    else if (wfend+1 < nfields)
                        apos = wfend+1;
                } else if (nfields > 0)
                    apos = 0;

                if (apos >= 0)
                    field.attr_name = field_strings[apos];

                split_string.erase(split_string.begin());
            }
            
            m_fields.push_back(field);
        }
    }

    void configure(const QuerySpec& spec) {
        if (spec.format.args.size() > 0)
            parse(spec.format.args[0]);
    }
    
    void print(CaliperMetadataAccessInterface& db, const EntryList& list) {
        std::ostringstream os;
        
        for (Field f : m_fields) {
            Attribute attr { Attribute::invalid };
            
            {
                std::lock_guard<std::mutex>
                    g(m_fields_lock);
                
                if (f.attr == Attribute::invalid)
                    f.attr = db.get_attribute(f.attr_name);

                attr = f.attr;
            }
            
            string str;

            if (attr != Attribute::invalid)
                for (const Entry& e: list) {
                    if (e.node()) {
                        for (const Node* node = e.node(); node; node = node->parent())
                            if (node->attribute() == attr.id())
                                str = node->data().to_string().append(str.size() ? "/" : "").append(str);
                    } else if (e.attribute() == attr.id())
                        str.append(e.value().to_string());

                    if (!str.empty())
                        break;
                }
            
            const char whitespace[80+1] =
                "                                        "
                "                                        ";

            size_t len = str.size();
            size_t w   = len < f.width ? std::min<size_t>(f.width - len, 80) : 0;

            os << f.prefix << str << (w > 0 ? whitespace+(80-w) : "");
        }

        {
            std::lock_guard<std::mutex>
                g(m_os_lock);

            std::ostream* real_os = m_os.stream();
            
            *real_os << os.str() <<std::endl;
        }
    }
};


UserFormatter::UserFormatter(OutputStream& os, const QuerySpec& spec)
    : mP { new FormatImpl(os) }
{
    mP->configure(spec);

    if (spec.format.args.size() > 1) {
        std::ostream* real_os = os.stream();
        *real_os << spec.format.args[1] << std::endl;
    }
}

UserFormatter::~UserFormatter()
{
    mP.reset();
}

void 
UserFormatter::process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->print(db, list);
}

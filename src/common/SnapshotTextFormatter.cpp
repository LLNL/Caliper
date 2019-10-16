// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file  SnapshotTextFormatter.cpp
/// \brief SnapshotTextFormatter implementation

#include "caliper/common/SnapshotTextFormatter.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"

#include "caliper/common/util/split.hpp"

#include <algorithm>
#include <iterator>
#include <mutex>
#include <vector>

using namespace cali;


struct cali::SnapshotTextFormatter::SnapshotTextFormatterImpl
{
    struct Field {
        std::string prefix;

        std::string attr_name;
        Attribute   attr;

        int         width;
        char        align; // 'l', 'r', 'c' 
    };

    std::vector<Field> m_fields;    
    std::mutex         m_field_mutex;

    void 
    parse(const std::string& formatstring) {
        m_fields.clear();

        // parse format: "(prefix string) %[<width+alignment(l|r|c)>]attr_name% ... "
        // FIXME: this is a very primitive parser

        std::vector<std::string> split_string;

        util::split(formatstring, '%', std::back_inserter(split_string));

        while (!split_string.empty()) {
            Field field = { "", "", Attribute::invalid, 0, 'l' };

            field.prefix = split_string.front();
            split_string.erase(split_string.begin());

            if (!split_string.empty()) {
                // parse field entry

                std::vector<std::string> field_strings;
                util::tokenize(split_string.front(), "[]", std::back_inserter(field_strings));
                
                // look for width/alignment specification (in [] brackets)

                int wfbegin = -1;
                int wfend   = -1;
                int apos    = -1;

                int nfields = field_strings.size(); 

                for (int i = 0; i < nfields; ++i)
                    if (field_strings[i] == "[")
                        wfbegin = i;
                    else if (field_strings[i] == "]")
                        wfend   = i;

                if (wfbegin >= 0 && wfend > wfbegin+1) {
                    // we have a width specification field
                    field.width = std::stoi(field_strings[wfbegin+1]);

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

    std::ostream& 
    print(std::ostream& os, const CaliperMetadataAccessInterface& db, const std::vector<Entry>& list) {
        std::vector<Field> fields;

        {
            std::lock_guard<std::mutex>
                g(m_field_mutex);

            fields.assign(m_fields.begin(), m_fields.end());
        }

        bool update = false;

        for (Field& f : fields) {
            if (!f.attr_name.empty()) {
                f.attr = db.get_attribute(f.attr_name);
                f.attr_name.clear();

                cali_attr_type type = f.attr.type();

                f.align = (type == CALI_TYPE_DOUBLE ||
                           type == CALI_TYPE_INT    ||
                           type == CALI_TYPE_UINT   ||
                           type == CALI_TYPE_ADDR) ? 'r' : 'l';
                
                update = true;
            }

            std::string str;

            if (f.attr != Attribute::invalid) {
                Entry e;

                for (auto it = list.begin(); it != list.end(); ++it)
                    if ((*it).count(f.attr)) {
                        e = *it;
                        break;
                    }

                if (e.node()) {
                    for (const Node* node = e.node(); node; node = node->parent())
                        if (node->attribute() == f.attr.id())
                            str = node->data().to_string().append(str.size() ? "/" : "").append(str);
                } else if (e.is_immediate()) {
                    str.append(e.value().to_string());
                }
            }

            static const char whitespace[80+1] = 
                "                                        "
                "                                        ";

            int len = str.size();
            int w   = len < f.width ? std::min<int>(f.width - len, 80) : 0;

            if (f.align == 'r')
                os << f.prefix << (w > 0 ? whitespace+(80-w) : "") << str;
            else
                os << f.prefix << str << (w > 0 ? whitespace+(80-w) : "");
        }

        if (update) {
            std::lock_guard<std::mutex>
                g(m_field_mutex);

            m_fields.swap(fields);
        }

        return os;
    }
};

SnapshotTextFormatter::SnapshotTextFormatter(const std::string& format_str)
    : mP(new SnapshotTextFormatterImpl)
{ 
    mP->parse(format_str);
}

SnapshotTextFormatter::~SnapshotTextFormatter()
{
    mP.reset();
} 

void
SnapshotTextFormatter::reset(const std::string& format_str)
{
    mP->parse(format_str);
}

std::ostream& 
SnapshotTextFormatter::print(std::ostream& os, const CaliperMetadataAccessInterface& db, const std::vector<Entry>& list)
{
    return mP->print(os, db, list);
}

/// \file  SnapshotTextFormatter.cpp
/// \brief SnapshotTextFormatter implementation

#include "SnapshotTextFormatter.h"

#include <Snapshot.h>

#include <Node.h>
#include <util/split.hpp>

#include <algorithm>
#include <iterator>
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

    void parse(const std::string& formatstring, Caliper* c) {
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

            field.attr = c->get_attribute(field.attr_name);

            if (field.attr != Attribute::invalid)
                field.attr_name.clear();

            m_fields.push_back(field);
        }
    }

    void update_attribute(const Attribute& attr) {
        std::string name = attr.name();

        for (Field& f : m_fields)
            if (!f.attr_name.empty() && name == f.attr_name) {
                f.attr = attr;
                f.attr_name.clear();
            }
    }

    std::ostream& print(std::ostream& os, const Snapshot* s) const {
        for (const Field& f : m_fields) {
            Entry e = Entry::empty;

            if (f.attr != Attribute::invalid)
                e = s->get(f.attr);

            std::string str;
            
            if (e.node()) {
                for (const Node* node = e.node(); node; node = node->parent())
                    if (node->attribute() == f.attr.id())
                        str = node->data().to_string().append(str.size() ? "/" : "").append(str);
            } else if (!e.is_empty())
                str.append(e.value().to_string());

            static const char whitespace[80+1] = 
                "                                        "
                "                                        ";

            int len = str.size();
            int w   = len < f.width ? std::min<int>(f.width - len, 80) : 0;

            os << f.prefix << str << (w > 0 ? whitespace+(80-w) : "");
        }

        return os;
    }
};

SnapshotTextFormatter::SnapshotTextFormatter()
    : mP(new SnapshotTextFormatterImpl)
{ }

SnapshotTextFormatter::~SnapshotTextFormatter()
{
    mP.reset();
} 

void
SnapshotTextFormatter::parse(const std::string& format_str, Caliper* c)
{
    mP->parse(format_str, c);
}

void 
SnapshotTextFormatter::update_attribute(const Attribute& attr)
{
    mP->update_attribute(attr);
}

std::ostream& 
SnapshotTextFormatter::print(std::ostream& os, const Snapshot* s) const
{
    return mP->print(os, s);
}

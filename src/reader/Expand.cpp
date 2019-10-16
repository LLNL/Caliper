// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Print expanded records

#include "caliper/reader/Expand.h"

#include "caliper/reader/QuerySpec.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/CaliperMetadataAccessInterface.h"
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

struct Expand::ExpandImpl
{
    set<string>  m_selected;
    set<string>  m_deselected;

    std::map<string, string> m_aliases;

    OutputStream m_os;

    std::mutex   m_os_lock;

    ExpandImpl(OutputStream& os)
        : m_os(os)
        { }

    void parse(const string& field_string) {
        vector<string> fields;

        util::split(field_string, ':', back_inserter(fields));

        for (const string& s : fields) {
            if (s.size() == 0)
                continue;

            if (s[0] == '-')
                m_deselected.insert(s.substr(1, string::npos));
            else
                m_selected.insert(s);
        }
    }

    void configure(const QuerySpec& spec) {
        switch (spec.attribute_selection.selection) {
        case QuerySpec::AttributeSelection::Default:
        case QuerySpec::AttributeSelection::All:
            // do nothing; default is all
            break;
        case QuerySpec::AttributeSelection::None:
            // doesn't make much sense
            break;
        case QuerySpec::AttributeSelection::List:
            m_selected =
                std::set<std::string>(spec.attribute_selection.list.begin(),
                                      spec.attribute_selection.list.end());
            break;
        }

        m_aliases = spec.aliases;
    }
    
    void print(CaliperMetadataAccessInterface& db, const EntryList& list) {
        int nentry = 0;

        std::ostringstream os;
        
        for (const Entry& e : list) {
            if (e.node()) {
                vector<const Node*> nodes;

                for (const Node* node = e.node(); node && node->attribute() != CALI_INV_ID; node = node->parent()) {
                    string name = db.get_attribute(node->attribute()).name();

                    if ((!m_selected.empty() && m_selected.count(name) == 0) || m_deselected.count(name))
                        continue;

                    nodes.push_back(node);
                }

                if (nodes.empty())
                    continue;

                stable_sort(nodes.begin(), nodes.end(), [](const Node* a, const Node* b) { return a->attribute() < b->attribute(); } );
	  
                cali_id_t prev_attr_id = CALI_INV_ID;

                for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
                    if ((*it)->attribute() != prev_attr_id) {
                        std::string name = db.get_attribute((*it)->attribute()).name();

                        {
                            auto it = m_aliases.find(name);
                            if (it != m_aliases.end())
                                name = it->second;
                        }
                        
                        os << (nentry++ ? "," : "") << name << '=';
                        prev_attr_id = (*it)->attribute();
                    } else {
                        os << '/';
                    }
                    os << (*it)->data().to_string();
                }
            } else if (e.attribute() != CALI_INV_ID) {
                string name = db.get_attribute(e.attribute()).name();

                if ((!m_selected.empty() && m_selected.count(name) == 0) || m_deselected.count(name))
                    continue;

                {
                    auto it = m_aliases.find(name);
                    if (it != m_aliases.end())
                        name = it->second;
                }

                os << (nentry++ ? "," : "") << name << '=' << e.value();
            }
        }
        
        if (nentry > 0) {
            std::lock_guard<std::mutex>
                g(m_os_lock);

            std::ostream* real_os = m_os.stream();
            
            *real_os << os.str() << endl;
        }
    }
};


Expand::Expand(OutputStream& os, const string& field_string)
    : mP { new ExpandImpl(os) }
{
    mP->parse(field_string);
}

Expand::Expand(OutputStream& os, const QuerySpec& spec)
    : mP { new ExpandImpl(os) }
{
    mP->configure(spec);
}

Expand::~Expand()
{
    mP.reset();
}

void 
Expand::operator()(CaliperMetadataAccessInterface& db, const EntryList& list) const
{
    mP->print(db, list);
}

void
Expand::process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->print(db, list);
}

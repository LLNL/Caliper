/// @file ContextRecord.cpp
/// ContextRecord implementation

#include "ContextRecord.h"

#include "Node.h"

using namespace cali;
using namespace std;

RecordMap
ContextRecord::unpack(const RecordMap& rec, std::function<const Node*(cali_id_t)> get_node)
{
    RecordMap out;

    auto entry_it = rec.find("__rec");

    if (entry_it == rec.end() || entry_it->second.empty() || entry_it->second.front().to_string() != "ctx")
        return out;

    // implicit entries: 

    entry_it = rec.find("implicit");

    if (entry_it != rec.end())
        for (const Variant& elem : entry_it->second) {
            const Node* node = get_node(elem.to_id());

            for ( ; node && node->id() != CALI_INV_ID; node = node->parent() ) {
                const Node* attr_node = get_node(node->attribute());

                if (attr_node)
                    out[attr_node->data().to_string()].push_back(node->data());
            }
        }

    auto expl_entry_it = rec.find("explicit");
    auto data_entry_it = rec.find("data");

    if (expl_entry_it == rec.end() || data_entry_it == rec.end() || expl_entry_it->second.empty())
        return out;

    for (unsigned i = 0; i < expl_entry_it->second.size() && i < data_entry_it->second.size(); ++i) {
        const Node* attr_node = get_node(expl_entry_it->second[i].to_id());

        if (attr_node)
            out[attr_node->data().to_string()].push_back(data_entry_it->second[i]);
    }

    return out;
}

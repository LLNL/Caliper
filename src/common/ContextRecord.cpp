/// @file ContextRecord.cpp
/// ContextRecord implementation

#include "ContextRecord.h"

#include "Node.h"

#include <string>
#include <vector>


using namespace cali;
using namespace std;


namespace 
{

RecordMap value_record(const Attribute& attr, uint64_t val)
{
    return { 
        { "attribute", { attr.id()   } },
        { "type",      { attr.type() } },
        { "data",      { val         } } };
}

} // anonymous namespace 


vector<RecordMap>
ContextRecord::unpack(std::function<Attribute  (ctx_id_t)> get_attr,
                      std::function<const Node*(ctx_id_t)> get_node,
                      const uint64_t buf[], size_t size)
{
    vector<RecordMap> vec;

    for (size_t i = 0; i < size / 2; ++i) {
        Attribute attr = get_attr(buf[2*i]);
        uint64_t  val  = buf[2*i+1];

        if (attr == Attribute::invalid) // Oops?! Shouldn't happen
            return vec;
            
        if (attr.store_as_value())
            vec.push_back(::value_record(attr, val));
        else {
            const Node* node = get_node(val);

            // unpack all nodes up to root
            while (node && !(attr == Attribute::invalid)) {
                vec.push_back(node->record());

                // Locking ... ? Should work though since this should be read-only data
                node = node->parent();
                attr = get_attr(node ? node->attribute() : CTX_INV_ID);
            }
        }
    }

    return vec;
}

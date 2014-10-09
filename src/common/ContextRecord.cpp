/// @file ContextRecord.cpp
/// ContextRecord implementation

#include "ContextRecord.h"

#include "NodePtrQuery.h"


using namespace cali;
using namespace std;

namespace 
{

class ValueQuery : public Query {
    Attribute m_attr;
    uint64_t  m_value;

public:

    ValueQuery(const Attribute& attr, uint64_t val)
        : m_attr { attr }, m_value { val }
        { }

    bool          valid() const override { 
        return !(m_attr == Attribute::invalid); 
    }

    ctx_id_t      attribute() const override      { return m_attr.id();   }
    std::string   attribute_name() const override { return m_attr.name(); }
    ctx_attr_type type() const override           { return m_attr.type(); }

    size_t        size() const override           { return sizeof(uint64_t); }
    const void*   data() const override           { return &m_value;      }
};

}


vector< unique_ptr<Query> >
ContextRecord::unpack(std::function<Attribute  (ctx_id_t)> get_attr,
                      std::function<const Node*(ctx_id_t)> get_node,
                      const uint64_t buf[], size_t size)
{
    vector< unique_ptr<Query> > vec;

    for (size_t i = 0; i < size / 2; ++i) {
        ctx_id_t  id   = buf[2*i];
        uint64_t  val  = buf[2*i+1];
        Attribute attr = get_attr(id);

        if (attr == Attribute::invalid) // Oops?! Shouldn't happen
            return vec;
            
        if (attr.store_as_value())
            vec.push_back(unique_ptr<Query>(new ::ValueQuery(attr, val)));
        else {
            const Node* node = get_node(val);

            // unpack all nodes up to root
            while (node && !(attr == Attribute::invalid)) {
                vec.push_back(unique_ptr<Query>(new NodePtrQuery(attr, node)));

                // Locking ... ? Should work though since this should be read-only data
                node = node->parent();
                id   = node ? node->attribute() : CTX_INV_ID;

                attr = get_attr(id);
            }
        }
    }

    return vec;
}

/// @file Node.cpp
/// Node class implementation

#include "Node.h"

#include "Attribute.h"

#include <cstring>

using namespace cali;
using namespace std;


Node::~Node()
{
    unlink();
}

bool Node::equals(cali_id_t attr, const void* data, size_t size) const
{
    if (m_attribute == attr)
        return m_data == Variant(m_data.type(), data, size);

    return false;
}

Record Node::rec() const
{
    static const Record::Key keys[6] = {
        Record::record_type_key, 
        { 0x01, "id"     },
        { 0x02, "attr"   },
        { 0x03, "data"   },
        { 0x04, "type"   },
        { 0x05, "parent" } 
    };

    cali_id_t parent_id = parent() ? parent()->id() : CALI_INV_ID;

    Variant data[6] = { { Record::first_user_id + 1   },
                        { id()          },
                        { m_attribute   }, 
                        m_data, 
                        { m_data.type() },
                        { parent_id     }
    };

    return Record(parent_id != CALI_INV_ID ? 6 : 5, keys, data);
}

RecordMap Node::record() const
{
    RecordMap recmap = {
        { "id",        { id() }          },
        { "attribute", { m_attribute }   },
        { "data",      { m_data }        } };

    if (m_data.type() != CALI_TYPE_INV)
        recmap.insert(make_pair( "type",   Variant(m_data.type())  ));
    if (parent() && parent()->id() != CALI_INV_ID)
        recmap.insert(make_pair( "parent", Variant(parent()->id()) ));

    return recmap;
}

/** 
 * @file node.h 
 * Node class declaration
 */

#ifndef CALI_NODE_H
#define CALI_NODE_H

#include "cali_types.h"

#include "IdType.h"
#include "Record.h"
#include "RecordMap.h"
#include "Variant.h"

#include "util/tree.hpp"


namespace cali 
{

//
// --- Node base class 
//

class Node : public IdType, public util::IntrusiveTree<Node> 
{
    util::IntrusiveTree<Node>::Node m_treenode;

    cali_id_t m_attribute;
    Variant   m_data;

    static const RecordDescriptor s_record;

public:

    Node(cali_id_t id, cali_id_t attr, const Variant& data)
        : IdType(id),
          util::IntrusiveTree<Node>(this, &Node::m_treenode), 
          m_attribute { attr }, m_data { data }
        { }


    Node(const Node&) = delete;

    Node& operator = (const Node&) = delete;

    ~Node();

    bool equals(cali_id_t attr, const Variant& v) const {
        return m_attribute == attr ? m_data == v : false;
    }

    bool equals(cali_id_t attr, const void* data, size_t size) const;

    cali_id_t attribute() const { return m_attribute; }
    Variant   data() const      { return m_data;      }    

    /// @name Serialization API
    /// @{

    void      push_record(WriteRecordFn recfn) const;
    static const RecordDescriptor& record_descriptor() { return s_record; }

    RecordMap record() const;

    /// @}
};

} // namespace cali

#endif

/** 
 * @file node.h 
 * Node class declaration
 */

#ifndef CALI_NODE_H
#define CALI_NODE_H

#include "cali_types.h"

#include "util/tree.hpp"


namespace cali {

//
// Forward declarations



//
// --- Node base class 
//

class Node : public IdType {
    ctx_id_t                  m_attribute;

    union {
        int64_t  int64;
        uint64_t uint64;
        double   dbl;
        char[8]  string8;
        char*    string256;
        void*    data;
    }                         m_value;

    util::IntrusiveTree<Node> m_tree;

public:

    Node(ctx_id_t id, std::size_t typesize);

    ~Node();

    void set_attribute(ctx_id_t attr) {
        m_attribute = attr;
    }

    ctx_id_t attribute() const {
        return m_attribute;
    }

    const void* value() const { return m_value.data } ;
};

} // namespace cali

#endif

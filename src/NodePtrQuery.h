/// @file NodePtrQuery.h
/// NodeQuery interface declaration

#ifndef CALI_NODEPTRQUERY_H
#define CALI_NODEPTRQUERY_H

#include "cali_types.h"

#include "Attribute.h"
#include "Node.h"
#include "Query.h"

#include <iostream>


namespace cali
{

// --- NodeQuery API

class NodePtrQuery : public NodeQuery
{
    cali::Attribute   m_attr;
    const cali::Node* m_node;

public:

    NodePtrQuery(const cali::Attribute& attr, const cali::Node* node)
        : m_attr { attr }, m_node { node }
        { }

    ~NodePtrQuery()
        { }

    ctx_id_t      attribute() const override      { return m_attr.id();    }
    std::string   attribute_name() const override { return m_attr.name();  }
    ctx_attr_type type() const override           { return m_attr.type();  }

    std::size_t   size() const override           { return m_node->size(); }
    const void*   data() const override           { return m_node->data(); }

    bool          valid() const override {
        return m_node && !(m_attr == Attribute::invalid);
    }

    ctx_id_t id() const { 
        return m_node->id(); 
    }
    ctx_id_t parent() const {
        cali::Node* node = m_node->parent();
        return node ? node->id() : CTX_INV_ID;
    }
    ctx_id_t first_child() const {
        cali::Node* node = m_node->first_child();
        return node ? node->id() : CTX_INV_ID;
    }
    ctx_id_t next_sibling() const {
        cali::Node* node = m_node->next_sibling();
        return node ? node->id() : CTX_INV_ID;
    }
};

}

#endif // CALI_NODEQUERY_H

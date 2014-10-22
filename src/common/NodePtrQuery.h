/// @file NodePtrQuery.h
/// NodeQuery interface declaration

#ifndef CALI_NODEPTRQUERY_H
#define CALI_NODEPTRQUERY_H

#include "cali_types.h"

#include "Attribute.h"
#include "Node.h"
#include "Query.h"
#include "Variant.h"

#include <iostream>

namespace cali
{

// --- NodeQuery API

class NodeRecord : public Record
{
    cali::Attribute   m_attr;
    const cali::Node* m_node;

public:

    NodePtrQuery(const cali::Attribute& attr, const cali::Node* node)
        : m_attr { attr }, m_node { node }
        { }

    ~NodePtrQuery()
        { }

    std::vector<std::string> keys() const override {
        return { 
            "attribute", "attribute_name", "data",  
                "first_child", "id", "next_sibling", "parent", "type" };
    }

    Variant get(const std::string& key) const override;

    bool valid() const override {
        return m_node && !(m_attr == Attribute::invalid);
    }
};

}

#endif // CALI_NODEQUERY_H

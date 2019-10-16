// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#ifndef CALI_ENTRY_H
#define CALI_ENTRY_H

#include "Attribute.h"
#include "Variant.h"

namespace cali
{

//
// --- Class for either a node or immediate data element
//

/// \brief Encapsulate a blackboard or snapshot record entry. 
///
/// Represents a blackboard or snapshot record entry, which can be either 
/// a _reference entry_ (reference into the context tree) or an 
/// _immediate entry_ (explicit key:value pair). Reference entries are
/// stored as context tree node pointer, immediate entries are stored as
/// (attribute id, value) pair.

class Entry 
{
    const Node* m_node;
    cali_id_t   m_attr_id;
    Variant     m_value;

public:

    Entry()
        : m_node(0), m_attr_id(CALI_INV_ID)
        { }

    Entry(const Node* node)
        : m_node(node), m_attr_id(CALI_INV_ID)
        { }

    Entry(cali_id_t id, const Variant& val)
        : m_node(0), m_attr_id(id), m_value(val)
        { }
    Entry(const Attribute& attr, const Variant& val)
        : m_node(0), m_attr_id(attr.id()), m_value(val)
        { }

    /// \brief Return context tree node pointer for reference entries.
    /// \return The context tree node. Null pointer if the entry is
    ///   an immediate entry or empty.
    const Node* node() const {
        return m_node;
    }
    
    /// \brief Return top-level attribute ID of this entry
    ///
    /// For immediate entries, returns the stored attribute id.
    /// For reference entries, returns the referenced node's 
    /// attribute id.
    cali_id_t attribute() const { return m_node ? m_node->attribute() : m_attr_id; }

    /// \brief Count instances of attribute \a attr_id in this entry
    int       count(cali_id_t attr_id = CALI_INV_ID) const;
    int       count(const Attribute& attr) const {
        return count(attr.id());
    }

    /// \brief Return top-level data value of this entry
    Variant   value() const { return m_node ? m_node->data() : m_value; }

    /// \brief Extract data value for attribute \a attr_id from this entry
    Variant   value(cali_id_t attr_id) const;
    Variant   value(const Attribute& attr) const {
        return value(attr.id());
    }

    bool      is_empty() const {
        return m_node == 0 && m_attr_id == CALI_INV_ID;
    }
    bool      is_immediate() const {
        return m_node == 0 && m_attr_id != CALI_INV_ID;
    }
    bool      is_reference() const {
        return m_node != 0;
    }

    static const Entry empty;

    friend bool operator == (const Entry&, const Entry&);
};

bool operator == (const Entry& lhs, const Entry& rhs);
    
} // namespace cali

#endif

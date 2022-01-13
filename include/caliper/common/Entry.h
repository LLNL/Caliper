// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#ifndef CALI_ENTRY_H
#define CALI_ENTRY_H

#include "Attribute.h"
#include "Variant.h"

#include "caliper/caliper-config.h"

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
/// (attribute, value) pair.

class Entry
{
    const Node* m_node;  // reference or attribute node
    Variant     m_value;

public:

    CONSTEXPR_UNLESS_PGI Entry()
        : m_node(nullptr)
        { }

    Entry(const Node* node)
        : m_node(node)
        { }

    Entry(const Attribute& attr, const Variant& val)
        : m_node(attr.node()), m_value(val)
        { }

    /// \brief Return context tree node pointer for reference entries.
    /// \return The context tree node or attribute node
    const Node* node() const {
        return m_node;
    }

    /// \brief Return top-level attribute ID of this entry
    ///
    /// For immediate entries, returns the stored attribute id.
    /// For reference entries, returns the referenced node's
    /// attribute id.
    cali_id_t attribute() const {
        return Attribute::is_attribute(m_node) ? m_node->id() : (m_node ? m_node->attribute() : CALI_INV_ID);
    };

    /// \brief Count instances of attribute \a attr_id in this entry
    int       count(cali_id_t attr_id = CALI_INV_ID) const;
    int       count(const Attribute& attr) const {
        return count(attr.id());
    }

    /// \brief Return top-level data value of this entry
    Variant   value() const {
        if (!m_node)
            return Variant();

        return Attribute::is_attribute(m_node) ? m_value : m_node->data();
    }

    /// \brief Extract data value for attribute \a attr_id from this entry
    Variant   value(cali_id_t attr_id) const;
    Variant   value(const Attribute& attr) const {
        return value(attr.id());
    }

    bool      is_empty() const {
        return m_node == nullptr;
    }
    bool      is_immediate() const {
        return Attribute::is_attribute(m_node);
    }
    bool      is_reference() const {
        return !is_empty() && !is_immediate();
    }

    static const Entry empty;

    friend bool operator == (const Entry&, const Entry&);
};

inline bool operator == (const Entry& lhs, const Entry& rhs)
{
    if (lhs.is_empty() || rhs.is_empty())
        return lhs.m_node == rhs.m_node;

    if (lhs.m_node->id() == rhs.m_node->id()) {
        if (Attribute::is_attribute(lhs.m_node))
            return lhs.m_value == rhs.m_value;
        else 
            return true;
    }

    return false;
}

} // namespace cali

#endif

// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once
#ifndef CALI_ENTRY_H
#define CALI_ENTRY_H

#include "Attribute.h"
#include "Variant.h"

#include "caliper/caliper-config.h"

namespace cali
{

class CaliperMetadataAccessInterface;

//
// --- Class for either a node or immediate data element
//

/// \brief Encapsulate a blackboard or snapshot record entry.
///
/// Represents a blackboard or snapshot record entry, which can be either
/// a _reference entry_ (reference into the context tree) or an
/// _immediate entry_ (explicit key:value pair). Reference entries are
/// stored as context tree node pointer, immediate entries are stored as
/// (attribute node pointer, value) pair.

class Entry
{
    Node*   m_node; // reference or attribute node
    Variant m_value;

public:

    /// @brief Maximum size in bytes of a packed entry
    constexpr static size_t MAX_PACKED_SIZE = 30;

    /// @brief Construct an empty Entry
    constexpr Entry() : m_node(nullptr) {}

    /// @brief Construct a reference entry from \a node
    Entry(Node* node) : m_node(node), m_value(node->data()) {}

    /// @brief Construct an immediate entry from \a attr, \a val
    Entry(const Attribute& attr, const Variant& val) : m_node(attr.node()), m_value(val) {}

    /// \brief Return context tree node pointer for reference entries.
    /// \return The context tree node or attribute node
    Node* node() const { return m_node; }

    /// \brief Return top-level attribute ID of this entry
    ///
    /// For immediate entries, returns the stored attribute id.
    /// For reference entries, returns the referenced node's
    /// attribute id.
    cali_id_t attribute() const
    {
        if (m_node) {
            cali_id_t node_attr_id = m_node->attribute();
            return node_attr_id == Attribute::NAME_ATTR_ID ? m_node->id() : node_attr_id;
        }
        return CALI_INV_ID;
    }

    /// \brief Count instances of attribute \a attr_id in this entry
    int count(cali_id_t attr_id = CALI_INV_ID) const;

    int count(const Attribute& attr) const { return count(attr.id()); }

    /// \brief Return top-level data value of this entry
    Variant value() const { return m_value; }

    /// \brief Extract data value for attribute \a attr_id from this entry
    Variant value(cali_id_t attr_id) const;

    Variant value(const Attribute& attr) const { return value(attr.id()); }

    /// \brief Find and return entry for the given attribute in this entries'
    ///    value or path
    Entry get(const Attribute& attr) const;

    bool empty() const { return m_node == nullptr; }

    bool is_immediate() const { return Attribute::is_attribute(m_node); }

    bool is_reference() const { return !empty() && !is_immediate(); }

    /// @brief Write a compact binary serialization of the entry into \a buffer
    /// @param buffer The target buffer. Must have at least \ref MAX_PACKED_SIZE bytes of free space.
    /// @return The actual number of bytes written into \a buffer
    size_t pack(unsigned char* buffer) const;

    /// @brief Deserialize a packed entry from \a buffer
    /// @param db The %Caliper metadata (context tree nodes, attributes) associated with this entry.
    /// @param buffer The source buffer. Must point to a packed entry.
    /// @param inc Returns the number of bytes read from the source buffer.
    /// @return The deserialized entry.
    static Entry unpack(const CaliperMetadataAccessInterface& db, const unsigned char* buffer, size_t* inc);

    friend bool operator== (const Entry&, const Entry&);
};

inline bool operator== (const Entry& lhs, const Entry& rhs)
{
    bool node_eq = lhs.m_node == rhs.m_node || (lhs.m_node && rhs.m_node && lhs.m_node->id() == rhs.m_node->id());
    bool is_imm  = lhs.is_immediate();

    return node_eq && (!is_imm || (is_imm && lhs.m_value == rhs.m_value));
}

inline bool operator!= (const Entry& lhs, const Entry& rhs)
{
    return !(lhs == rhs);
}

} // namespace cali

#endif

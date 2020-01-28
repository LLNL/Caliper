// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#include "caliper/common/cali_types.h"

#include <memory>

namespace cali
{
    class  Attribute;
    struct MetaAttributeIDs;
    class  Node;
    class  Variant;
    
    class MetadataTree
    {
        struct MetadataTreeImpl;
        
        std::unique_ptr<MetadataTreeImpl> mP;
        
    public:
        
        MetadataTree();

        ~MetadataTree();

        MetadataTree(const MetadataTree&) = delete;
        MetadataTree& operator = (const MetadataTree&) = delete;

        static void release();
        
        // --- Modifying tree operations ---

        /// \brief Get or construct a path in the tree under parent with
        ///   the given attribute:data pairs
        Node*
        get_path(size_t n, const Attribute attr[], const Variant data[], Node* parent);

        /// \brief Get or construct a path in the tree under parent with
        ///   the attribute \a attr and the list of values in \a data
        Node*
        get_path(const Attribute& attr, size_t n, const Variant data[], Node* parent);

        /// \brief Get or construct a path in the tree under parent with
        ///   the data of the nodes given in the nodelist in the order of that list
        Node*
        get_path(size_t n, const Node* nodelist[], Node* parent);
        
        Node*
        remove_first_in_path(Node* path, const Attribute& attr);
        
        Node*
        replace_first_in_path(Node* path, const Attribute& attr, const Variant& data);
        Node*
        replace_all_in_path(Node* path, const Attribute& attr, size_t n, const Variant data[]);

        // --- Non-modifying tree operations

        Node*
        find_node_with_attribute(const Attribute& attr, Node* path) const;
        
        // --- Data access ---
        
        Node*
        node(cali_id_t) const;
        Node*
        root() const;

        Node*
        type_node(cali_attr_type type) const;

        // --- I/O ---

        std::ostream&
        print_statistics(std::ostream& os) const;
    };

} // namespace cali

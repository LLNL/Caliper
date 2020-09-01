// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file CaliperMetadataDB.h
/// \brief CaliperMetadataDB class declaration

#ifndef CALI_CALIPERMETADATADB_H
#define CALI_CALIPERMETADATADB_H

#include "RecordProcessor.h"

#include "../common/Attribute.h"
#include "../common/CaliperMetadataAccessInterface.h"

#include <map>
#include <memory>
#include <string>

namespace cali
{

class Node;
class Variant;

typedef std::map<cali_id_t, cali_id_t> IdMap;

/// \brief Maintains a context tree and provides metadata information.
/// \ingroup ReaderAPI

class CaliperMetadataDB : public CaliperMetadataAccessInterface
{
    struct CaliperMetadataDBImpl;
    std::unique_ptr<CaliperMetadataDBImpl> mP;

public:

    CaliperMetadataDB();

    ~CaliperMetadataDB();

    //
    // --- I/O API
    //

    const Node* merge_node    (cali_id_t       node_id,
                               cali_id_t       attr_id,
                               cali_id_t       prnt_id,
                               const Variant&  v_data,
                               IdMap&          idmap);

    const Node* merge_node    (cali_id_t       node_id,
                               cali_id_t       attr_id,
                               cali_id_t       prnt_id,
                               const std::string& data,
                               IdMap&          idmap);

    EntryList   merge_snapshot(size_t          n_nodes,
                               const cali_id_t node_ids[],
                               size_t          n_imm,
                               const cali_id_t attr_ids[],
                               const Variant   values[],
                               const IdMap&    idmap) const;

    /// \brief Merge snapshot record bound to metadata DB \a db
    ///   into this metadata DB
    EntryList   merge_snapshot(const CaliperMetadataAccessInterface& db,
                               const std::vector<Entry>& rec);

    Entry       merge_entry   (cali_id_t       node_id,
                               const IdMap&    idmap);
    Entry       merge_entry   (cali_id_t       attr_id,
                               const std::string& data,
                               const IdMap&    idmap);

    void        merge_global  (cali_id_t       node_id,
                               const IdMap&    idmap);
    void        merge_global  (cali_id_t       attr_id,
                               const std::string& data,
                               const IdMap&    idmap);

    //
    // --- Query API
    //

    Node*       node(cali_id_t id) const;

    Attribute   get_attribute(cali_id_t id) const;
    Attribute   get_attribute(const std::string& name) const;

    std::vector<Attribute> get_all_attributes() const;

    //
    // --- Manipulation
    //

    Node*       make_tree_entry(std::size_t n, const Node* nodelist[], Node* parent = 0);
    Node*       make_tree_entry(std::size_t n, const Attribute attr[], const Variant data[], Node* parent = 0);

    Attribute   create_attribute(const std::string& name,
                                 cali_attr_type     type,
                                 int                prop,
                                 int                meta = 0,
                                 const Attribute*   meta_attr = nullptr,
                                 const Variant*     meta_data = nullptr);
    //
    // --- Globals
    //

    /// \brief Get global entries (entries with the CALI_ATTR_GLOBAL flag)
    std::vector<Entry> get_globals();

    /// \brief Set a global entry
    void               set_global(const Attribute& attr, const Variant& value);

    /// \brief Import global entries from metadata DB \a db into this
    ///   metadata DB
    std::vector<Entry> import_globals(CaliperMetadataAccessInterface& db);

    /// \brief Import globals in record \a rec from metadata DB \a db into this
    ///   metadata DB
    std::vector<Entry> import_globals(CaliperMetadataAccessInterface& db, const std::vector<Entry>& globals);

    /// \brief Add a set of attribute aliases
    ///
    /// This adds a "attribute.alias" meta-attribute for the aliased attribute
    /// to export alias information in a cali data stream.
    /// Currently this is limited to new attributes created with
    /// create_attribute() in this database. It does not apply to imported
    /// attributes.
    void add_attribute_aliases(const std::map<std::string, std::string>& aliases);

    /// \brief Add a set of attribute units
    ///
    /// This adds a "attribute.unit" meta-attribute for the aliased attribute
    /// to export alias information in a cali data stream.
    /// Currently this is limited to new attributes created with
    /// create_attribute() in this database. It does not apply to imported
    /// attributes.
    void add_attribute_units(const std::map<std::string, std::string>& aliases);

    /// \brief print usage statistics
    std::ostream&
    print_statistics(std::ostream& os);
};

}

#endif

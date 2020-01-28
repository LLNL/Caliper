// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/common/Attribute.h"
#include "caliper/common/Node.h"
#include "caliper/common/Variant.h"

#include "caliper/caliper-config.h"
#include "caliper/common/util/spinlock.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>

namespace cali
{

class CompressedSnapshotRecord;
class SnapshotRecord;

class Blackboard {
    constexpr static size_t Nmax = 1021;
    constexpr static size_t jump = 7;
    
    struct blackboard_entry_t {
        cali_id_t id    { CALI_INV_ID };
        
        enum {
            Empty = 0, ReferenceEntry, ImmediateEntry
        }         state { Empty };
        
        union blackboard_entry_data_t {
            CONSTEXPR_UNLESS_PGI blackboard_entry_data_t() : immediate() {};
            CONSTEXPR_UNLESS_PGI blackboard_entry_data_t(const cali::Variant& iref) : immediate(iref) {};
            cali::Variant immediate;
            cali::Node*   reference { nullptr };
        }         data;
    };

    blackboard_entry_t hashtable[Nmax];
    
    int      ref_toc[(Nmax+31)/32];
    int      ref_toctoc;

    int      imm_toc[(Nmax+31)/32];
    int      imm_toctoc;

    size_t   num_entries;
    size_t   max_num_entries;

    size_t   num_skipped;

    std::atomic<int>   ucount; // update count

    mutable util::spinlock lock;
    
    inline size_t find_existing_entry(cali_id_t id) const {
        size_t I = id % Nmax;

        while (hashtable[I].state != blackboard_entry_t::Empty && hashtable[I].id != id)
            I = (I + jump) % Nmax;            

        return I;
    }

    inline size_t find_free_slot(cali_id_t id) const {
        size_t I = id % Nmax;

        while (hashtable[I].state != blackboard_entry_t::Empty)
            I = (I + jump) % Nmax;            

        return I;
    }

    void add_entry(const cali::Attribute& attr, const cali::Variant& val);
    void add_entry(const cali::Attribute& attr, cali::Node* node);
    
public:

    Blackboard()
        : hashtable       {   },
          ref_toc         { 0 },
          ref_toctoc      { 0 },
          imm_toc         { 0 },
          imm_toctoc      { 0 },
          num_entries     { 0 },
          max_num_entries { 0 },
          num_skipped     { 0 },
          ucount          { 0 } 
        { }

    inline cali::Variant
    get(const cali::Attribute& attr) const {
        std::lock_guard<util::spinlock>
            g(lock);
        
        size_t I = find_existing_entry(attr.id());

        if (hashtable[I].id != attr.id())
            return Variant();

        if (attr.store_as_value())
            return hashtable[I].data.immediate;
        else
            return hashtable[I].data.reference->data();
    }

    inline cali::Node*
    get_node(const cali::Attribute& attr) const {
        std::lock_guard<util::spinlock>
            g(lock);

        size_t I = find_existing_entry(attr.id());
        
        return hashtable[I].id == attr.id() ? hashtable[I].data.reference : nullptr;
    }

    inline Variant
    get_immediate(const cali::Attribute& attr) const {
        std::lock_guard<util::spinlock>
            g(lock);

        size_t I = find_existing_entry(attr.id());
        
        return hashtable[I].id == attr.id() ? hashtable[I].data.immediate : Variant();
    }

    void    set(const cali::Attribute& attr, const cali::Variant& val);
    void    set(const cali::Attribute& attr, cali::Node* node);

    void    unset(const Attribute& attr);

    Variant exchange(const Attribute& attr, const cali::Variant& value);

    void    snapshot(CompressedSnapshotRecord* rec) const;
    void    snapshot(SnapshotRecord* rec) const;

    size_t  num_skipped_entries() const { return num_skipped; }

    int     count() const { return ucount.load(); }

    std::ostream& print_statistics(std::ostream& os) const;
};

} // namespace cali

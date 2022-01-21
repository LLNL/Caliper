// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#ifndef CALI_BLACKBOARD_H
#define CALI_BLACKBOARD_H

#include "caliper/common/Attribute.h"
#include "caliper/common/Entry.h"
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

class SnapshotBuilder;

class Blackboard {
    constexpr static size_t Nmax = 1021;

    struct blackboard_entry_t {
        cali_id_t key   { CALI_INV_ID };
        bool      is_occupied { false };
        Entry     value { };
    };

    blackboard_entry_t hashtable[Nmax];

    //   The toc ("table of contents") array is a bitfield that
    // indicates which elements in the hashtable are occupied. We use
    // it to speed up iterating over all entries in snapshot().
    //   Similarly, toctoc indicates which elements in toc are
    // occupied.
    int      toc[(Nmax+31)/32];
    int      toctoc;

    size_t   num_entries;
    size_t   max_num_entries;

    size_t   num_skipped;

    std::atomic<int>   ucount; // update count

    mutable util::spinlock lock;

    inline size_t find_existing_entry(cali_id_t key) const {
        size_t I = key % Nmax;

        while (hashtable[I].is_occupied && hashtable[I].key != key)
            I = (I+1) % Nmax;

        return I;
    }

    inline size_t find_free_slot(cali_id_t key) const {
        size_t I = key % Nmax;

        while (hashtable[I].is_occupied)
            I = (I+1) % Nmax;

        return I;
    }

    void add(cali_id_t key, const Entry& value, bool include_in_snapshot);

public:

    Blackboard()
        : hashtable       {   },
          toc             { 0 },
          toctoc          { 0 },
          num_entries     { 0 },
          max_num_entries { 0 },
          num_skipped     { 0 },
          ucount          { 0 }
        { }

    inline Entry
    get(cali_id_t key) const {
        std::lock_guard<util::spinlock>
            g(lock);

        size_t I = find_existing_entry(key);

        if (!hashtable[I].is_occupied || hashtable[I].key != key)
            return Entry();

        return hashtable[I].value;
    }

    void    set(cali_id_t key, const Entry& value, bool include_in_snapshots);
    void    del(cali_id_t key);

    Entry   exchange(cali_id_t key, const Entry& value, bool include_in_snapshots);

    void    snapshot(SnapshotBuilder& rec) const;

    size_t  num_skipped_entries() const { return num_skipped; }

    int     count() const { return ucount.load(); }

    std::ostream& print_statistics(std::ostream& os) const;
};

} // namespace cali

#endif

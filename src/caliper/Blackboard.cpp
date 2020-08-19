// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "Blackboard.h"

#include "caliper/SnapshotRecord.h"

#include "caliper/common/CompressedSnapshotRecord.h"

#include <cstring>
#include <iostream>

using namespace cali;

void
Blackboard::add_entry(const Attribute& attr, const Variant& val)
{
    cali_id_t attr_id = attr.id();
    size_t I = find_free_slot(attr_id);

    if (num_entries + (Nmax/10+10) > Nmax) {
        ++num_skipped; // Uh oh, we're full
        return;
    }

    hashtable[I].id    = attr_id;
    hashtable[I].state = blackboard_entry_t::ImmediateEntry;
    hashtable[I].data.immediate = val;

    if (!attr.is_hidden()) {
        imm_toc[I/32] |= (1 << (I%32));
        imm_toctoc    |= (1 << (I/32));
    }

    ++num_entries;
    max_num_entries = std::max(num_entries, max_num_entries);
}

void
Blackboard::add_entry(const Attribute& attr, cali::Node* node)
{
    cali_id_t attr_id = attr.id();
    size_t I = find_free_slot(attr_id);

    if (num_entries + (Nmax/10+10) > Nmax) {
        ++num_skipped; // Uh oh, we're full
        return;
    }

    hashtable[I].id    = attr_id;
    hashtable[I].state = blackboard_entry_t::ReferenceEntry;
    hashtable[I].data.reference = node;

    if (!attr.is_hidden()) {
        ref_toc[I/32] |= (1 << (I%32));
        ref_toctoc    |= (1 << (I/32));
    }

    ++num_entries;
    max_num_entries = std::max(num_entries, max_num_entries);
}

void
Blackboard::set(const Attribute& attr, const Variant& val)
{
    std::lock_guard<util::spinlock>
        g(lock);

    cali_id_t attr_id = attr.id();
    size_t I = find_existing_entry(attr_id);

    if (hashtable[I].id == attr_id)
        hashtable[I].data.immediate = val;
    else
        add_entry(attr, val);

    ++ucount;
}

void
Blackboard::set(const Attribute& attr, Node* node)
{
    std::lock_guard<util::spinlock>
        g(lock);

    size_t I = find_existing_entry(attr.id());

    if (hashtable[I].id == attr.id())
        hashtable[I].data.reference = node;
    else
        add_entry(attr, node);

    ++ucount;
}

void
Blackboard::unset(const Attribute& attr)
{
    std::lock_guard<util::spinlock>
        g(lock);

    size_t I = find_existing_entry(attr.id());

    if (hashtable[I].id != attr.id())
        return;

    hashtable[I].id    = CALI_INV_ID;
    hashtable[I].state = blackboard_entry_t::Empty;
    hashtable[I].data.immediate = Variant();

    --num_entries;
    ++ucount;

    if (attr.is_hidden())
        return;

    if (attr.store_as_value()) {
        imm_toc[I/32] &= ~(1 << (I%32));

        if (imm_toc[I/32] == 0)
            imm_toctoc &= ~(1 << (I/32));
    } else {
        ref_toc[I/32] &= ~(1 << (I%32));

        if (ref_toc[I/32] == 0)
            ref_toctoc &= ~(1 << (I/32));
    }
}

Variant
Blackboard::exchange(const Attribute& attr, const Variant& value)
{
    std::lock_guard<util::spinlock>
        g(lock);

    size_t  I = find_existing_entry(attr.id());
    Variant ret(cali_make_empty_variant());

    if (hashtable[I].id == attr.id()) {
        ret = hashtable[I].data.immediate;
        hashtable[I].data.immediate = value;
    } else
        add_entry(attr, value);

    ++ucount;

    return ret;
}

void
Blackboard::snapshot(CompressedSnapshotRecord* rec) const
{
    std::lock_guard<util::spinlock>
        g(lock);

    // reference entries

    {
        int tmptoc = ref_toctoc;

        while (tmptoc) {
            int i = ffs(tmptoc) - 1;
            tmptoc &= ~(1 << i);

            int tmp = ref_toc[i];

            while (tmp) {
                int j = ffs(tmp) - 1;
                tmp &= ~(1 << j);

                const blackboard_entry_t* entry = hashtable+(i*32+j);

                rec->append(1, &(entry->data.reference));
            }
        }
    }

    // immediate entries

    {
        int tmptoc = imm_toctoc;

        while (tmptoc) {
            int i = ffs(tmptoc) - 1;
            tmptoc &= ~(1 << i);

            int tmp = imm_toc[i];

            while (tmp) {
                int j = ffs(tmp) - 1;
                tmp &= ~(1 << j);

                const blackboard_entry_t* entry = hashtable+(i*32+j);

                rec->append(1, &(entry->id), &(entry->data.immediate));
            }
        }
    }
}

void
Blackboard::snapshot(SnapshotRecord* rec) const
{
    std::lock_guard<util::spinlock>
        g(lock);

    // reference entries

    {
        int tmptoc = ref_toctoc;

        while (tmptoc) {
            int i = ffs(tmptoc) - 1;
            tmptoc &= ~(1 << i);

            int tmp = ref_toc[i];

            while (tmp) {
                int j = ffs(tmp) - 1;
                tmp &= ~(1 << j);

                const blackboard_entry_t* entry = hashtable+(i*32+j);

                rec->append(entry->data.reference);
            }
        }
    }

    // immediate entries

    {
        int tmptoc = imm_toctoc;

        while (tmptoc) {
            int i = ffs(tmptoc) - 1;
            tmptoc &= ~(1 << i);

            int tmp = imm_toc[i];

            while (tmp) {
                int j = ffs(tmp) - 1;
                tmp &= ~(1 << j);

                const blackboard_entry_t* entry = hashtable+(i*32+j);

                rec->append(entry->id, entry->data.immediate);
            }
        }
    }
}

std::ostream&
Blackboard::print_statistics(std::ostream& os) const
{
    os << "max " << max_num_entries
       << " entries (" << 100.0*max_num_entries/Nmax << "% occupancy).";

    if (num_skipped > 0)
        os << num_skipped << " entries skipped!";

    return os;
}

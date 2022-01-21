// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "Blackboard.h"

#include "caliper/SnapshotRecord.h"

#include <iostream>

#ifdef _WIN32
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
namespace {
int first_high_bit(int x)
{
    unsigned long const mask = static_cast<unsigned long>(x);
    unsigned long index;
    unsigned char is_non_zero = _BitScanForward(&index, mask);
    if (is_non_zero) {
        return index + 1;
    }
    return 0;
}
}
#else
#include <strings.h>
namespace {
int first_high_bit(int x)
{
    return ffs(x);
}
}
#endif

using namespace cali;

void
Blackboard::add(cali_id_t key, const Entry& value, bool include_in_snapshots)
{
    size_t I = find_free_slot(key);

    if (num_entries + (Nmax/10+10) > Nmax) {
        ++num_skipped; // Uh oh, we're full
        return;
    }

    hashtable[I].key   = key;
    hashtable[I].is_occupied = true;
    hashtable[I].value = value;

    if (include_in_snapshots) {
        toc[I/32] |= (1 << (I%32));
        toctoc    |= (1 << (I/32));
    }

    ++num_entries;
    max_num_entries = std::max(num_entries, max_num_entries);
}

void
Blackboard::set(cali_id_t key, const Entry& value, bool include_in_snapshots)
{
    std::lock_guard<util::spinlock>
        g(lock);

    size_t I = find_existing_entry(key);

    if (hashtable[I].key == key)
        hashtable[I].value = value;
    else
        add(key, value, include_in_snapshots);

    ++ucount;
}

void
Blackboard::del(cali_id_t key)
{
    std::lock_guard<util::spinlock>
        g(lock);

    size_t I = find_existing_entry(key);

    if (!hashtable[I].is_occupied || hashtable[I].key != key)
        return;

    {
        size_t j = I;
        while (true) {
            j = (j+1) % Nmax;
            if (!hashtable[j].is_occupied)
                break;
            size_t k = hashtable[j].key % Nmax;
            if ((j > I && (k <= I || k > j)) || (j < I && (k <= I && k > j))) {
                hashtable[I] = hashtable[j];
                I = j;
            }
        }
    }

    hashtable[I].key   = CALI_INV_ID;
    hashtable[I].is_occupied = false;
    hashtable[I].value = Entry();

    --num_entries;
    ++ucount;

    toc[I/32] &= ~(1 << (I%32));
    if (toc[I/32] == 0)
        toctoc &= ~(1 << (I/32));
}

Entry
Blackboard::exchange(cali_id_t key, const Entry& value, bool include_in_snapshots)
{
    std::lock_guard<util::spinlock>
        g(lock);

    size_t  I = find_existing_entry(key);
    Entry ret;

    if (hashtable[I].key == key) {
        ret = hashtable[I].value;
        hashtable[I].value = value;
    } else
        add(key, value, include_in_snapshots);

    ++ucount;

    return ret;
}

void
Blackboard::snapshot(SnapshotBuilder& rec) const
{
    std::lock_guard<util::spinlock>
        g(lock);

    int tmptoc = toctoc;

    while (tmptoc) {
        int i = first_high_bit(tmptoc) - 1;
        tmptoc &= ~(1 << i);

        int tmp = toc[i];
        while (tmp) {
            int j = first_high_bit(tmp) - 1;
            tmp &= ~(1 << j);

            rec.append(hashtable[i*32+j].value);
        }
    }
}

std::ostream&
Blackboard::print_statistics(std::ostream& os) const
{
    os << "max " << max_num_entries
       << " entries (" << 100.0*max_num_entries/Nmax << "% occupancy).";

    if (num_skipped > 0)
        os << " " << num_skipped << " entries skipped!";

    return os;
}

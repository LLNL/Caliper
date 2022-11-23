// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file SnapshotRecord.h
/// \brief Snapshot record representation.

#pragma once

#include "common/Entry.h"

#include <algorithm>

namespace cali
{

class CaliperMetadataAccessInterface;

/// \brief Non-owning read-only view for snapshot record
class SnapshotView
{
    const Entry* m_data;
    size_t m_len;

public:

    SnapshotView()
        : m_data { nullptr }, m_len { 0 }
    { }
    SnapshotView(size_t len, const Entry* data)
        : m_data { data }, m_len { len }
    { }
    SnapshotView(const Entry& e)
        : m_data { &e }, m_len { 1 }
    { }

    using iterator = Entry*;
    using const_iterator = const Entry*;

    const_iterator begin() const { return m_data;       }
    const_iterator end()   const { return m_data+m_len; }

    size_t size()  const { return m_len;      }
    const Entry* data() const { return m_data;     }
    bool   empty() const { return m_len == 0; }

    const Entry& operator[](std::size_t n) const { return m_data[n]; }

    Entry get(const Attribute& attr) const {
        for (const Entry& e : *this) {
            Entry ret = e.get(attr);
            if (!ret.empty())
                return ret;
        }

        return Entry();
    }
};

/// \brief Non-owning writable view for snapshot record
class SnapshotBuilder
{
    Entry* m_data;
    size_t m_capacity;
    size_t m_len;
    size_t m_skipped;

public:

    SnapshotBuilder()
        : m_data { nullptr}, m_capacity { 0 }, m_len { 0 }, m_skipped { 0 }
    { }
    SnapshotBuilder(size_t capacity, Entry* data)
        : m_data { data }, m_capacity { capacity }, m_len { 0 }, m_skipped { 0 }
    { }

    SnapshotBuilder(SnapshotBuilder&&) = default;
    SnapshotBuilder(const SnapshotBuilder&) = delete;

    SnapshotBuilder& operator = (SnapshotBuilder&&) = default;
    SnapshotBuilder& operator = (const SnapshotBuilder&) = delete;

    size_t capacity() const { return m_capacity; }
    size_t size() const     { return m_len;      }
    size_t skipped() const  { return m_skipped;  }

    void append(const Entry& e) {
        if (m_len < m_capacity)
            m_data[m_len++] = e;
        else
            ++m_skipped;
    }

    void append(size_t n, const Entry* entries) {
        size_t num_copied = std::min(n, m_capacity - m_len);
        std::copy_n(entries, num_copied, m_data+m_len);
        m_len += num_copied;
        m_skipped += n-num_copied;
    }

    void append(const Attribute& attr, const Variant& val) {
        append(Entry(attr, val));
    }

    void append(SnapshotView view) {
        append(view.size(), view.data());
    }

    SnapshotView view() const {
        return SnapshotView { m_len, m_data };
    }
};

/// \brief A fixed-size snapshot record
template <std::size_t N>
class FixedSizeSnapshotRecord
{
    Entry m_data[N];
    SnapshotBuilder m_builder;

public:

    FixedSizeSnapshotRecord()
        : m_builder { N, m_data }
    { }

    FixedSizeSnapshotRecord(const FixedSizeSnapshotRecord<N>&) = delete;
    FixedSizeSnapshotRecord<N> operator = (const FixedSizeSnapshotRecord<N>&) = delete;

    SnapshotBuilder& builder() { return m_builder; }
    SnapshotView view() const  { return m_builder.view(); }

    void reset() {
        m_builder = SnapshotBuilder(N, m_data);
    }
};

} // namespace cali

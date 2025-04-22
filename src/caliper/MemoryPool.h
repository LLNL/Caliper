// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

///
/// @file MemoryPool.h
/// MemoryPool class declaration
///

#ifndef CALI_MEMORYPOOL_H
#define CALI_MEMORYPOOL_H

#include <iostream>
#include <memory>

namespace cali
{

class Node;

///
/// class DataPool
/// Memory management

class MemoryPool
{
    struct MemoryPoolImpl;
    std::shared_ptr<MemoryPoolImpl> mP;

public:

    MemoryPool();
    MemoryPool(std::size_t bytes);

    ~MemoryPool();

    // --- allocate

    void* allocate(std::size_t bytes);
    void* allocate(std::size_t bytes, std::size_t alignment);

    template <typename T>
    T* aligned_alloc(std::size_t n = 1, std::size_t a = alignof(T))
    {
        /*
        // ancient gcc 4.9-based STL versions don't have std::align :-((
        std::size_t size = n * sizeof(T);
        std::size_t space = n * sizeof(T) + a;
        void* p = allocate(space);
        return reinterpret_cast<T*>(std::align(a, size, p, space));
*/
        return reinterpret_cast<T*>(allocate(n * sizeof(T), a));
    }

    /// \brief Move \a other's data into this mempool.
    ///
    /// \note Does not lock \a other: we assume only one thread accesses it.
    void merge(MemoryPool& other);

    std::ostream& print_statistics(std::ostream& os) const;
};

} // namespace cali

#endif // CALI_MEMORYPOOL_H

// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
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

    std::ostream& print_statistics(std::ostream& os) const;
};

} // namespace cali

#endif // CALI_MEMORYPOOL_H

///
/// @file MemoryPool.h
/// MemoryPool class declaration
///

#ifndef CALI_MEMORYPOOL_H
#define CALI_MEMORYPOOL_H

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

    std::unique_ptr<MemoryPoolImpl> mP;


public:

    MemoryPool();
    MemoryPool(std::size_t bytes);

    ~MemoryPool();

    // --- allocate 

    void* allocate(std::size_t bytes);
};

} // namespace cali

#endif // CALI_MEMORYPOOL_H

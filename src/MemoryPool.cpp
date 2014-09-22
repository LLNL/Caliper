/// @file MemoryPool.cpp
/// Memory pool class definition

#include "MemoryPool.h"

#include <algorithm>
#include <cstring>
#include <vector>


using namespace cali;
using namespace std;


struct MemoryPool::MemoryPoolImpl
{
    // --- data

    const size_t chunksize = 64 * 1024;

    template<typename T> 
    struct Chunk {
        T*     ptr;
        size_t wmark;
        size_t size;
    };
        
    vector< Chunk<uint64_t> > chunks;

    size_t index;

    void expand(size_t bytes) {
        size_t len = max((bytes+sizeof(uint64_t)-1)/sizeof(uint64_t), chunksize);

        chunks.push_back( { new uint64_t[len], 0, len } );

        index = chunks.size() - 1;
    }

    void* allocate(size_t bytes, bool can_expand) {
        size_t n = (bytes+sizeof(uint64_t)-1)/sizeof(uint64_t);

        if (index == chunks.size() || chunks[index].wmark + n > chunks[index].size) {
            if (can_expand)
                expand(bytes);
            else
                return nullptr;
        }

        void *ptr = static_cast<void*>(chunks[index].ptr + chunks[index].wmark);
        chunks[index].wmark += n;

        return ptr;
    }

    MemoryPoolImpl() 
        : index { 0 } {
    }

    ~MemoryPoolImpl() {
        for ( auto &c : chunks )
            delete[] c.ptr;

        chunks.clear();
    }
};


MemoryPool::MemoryPool(size_t bytes)
    : mP { new MemoryPoolImpl }
{ 
    mP->expand(bytes);
}

MemoryPool::~MemoryPool()
{
    mP.reset();
}

void* MemoryPool::allocate(size_t bytes)
{
    return mP->allocate(bytes, false);
}

// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "MemoryPool.h"

#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/c-util/unitfmt.h"

#include "caliper/common/util/spinlock.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <vector>


using namespace cali;
using namespace std;


struct MemoryPool::MemoryPoolImpl
{
    // --- data

    const size_t chunksize = 64 * 1024;

    static const ConfigSet::Entry s_configdata[];

    template<typename T> 
    struct Chunk {
        T*     ptr;
        size_t wmark;
        size_t size;
    };

    ConfigSet                 m_config;

    util::spinlock            m_lock;
        
    vector< Chunk<uint64_t> > m_chunks;
    size_t                    m_index;
    bool                      m_can_expand;

    size_t                    m_total_reserved;
    size_t                    m_total_used;
    
    // --- interface 

    void expand(size_t bytes) {
        size_t len = max((bytes+sizeof(uint64_t)-1)/sizeof(uint64_t), chunksize);

        m_chunks.push_back( { new uint64_t[len], 0, len } );

        m_index = m_chunks.size() - 1;
        m_total_reserved += len;
    }

    void* allocate(size_t bytes, bool can_expand) {
        size_t n = (bytes+sizeof(uint64_t)-1)/sizeof(uint64_t);

        std::lock_guard<util::spinlock> lock(m_lock);
        
        if (m_index == m_chunks.size() || m_chunks[m_index].wmark + n > m_chunks[m_index].size) {
            if (can_expand)
                expand(bytes);
            else
                return nullptr;
        }

        void *ptr = static_cast<void*>(m_chunks[m_index].ptr + m_chunks[m_index].wmark);
        m_chunks[m_index].wmark += n;

        m_total_used += n;
        return ptr;
    }

    std::ostream& print_statistics(std::ostream& os) const {
        unitfmt_result bytes_reserved 
            = unitfmt(m_total_reserved, unitfmt_bytes);
        unitfmt_result bytes_used     
            = unitfmt(m_total_used,     unitfmt_bytes);

        os << "Metadata memory pool: "
           << bytes_reserved.val << " " << bytes_reserved.symbol << " reserved, "
           << bytes_used.val     << " " << bytes_used.symbol     << " used";

        return os;
    }
    
    MemoryPoolImpl() 
        : m_config { RuntimeConfig::get_default_config().init("memory", s_configdata) },
          m_index  { 0 },
          m_total_reserved { 0 }, m_total_used { 0 }
    {
        m_can_expand = m_config.get("can_expand").to_bool();
        size_t s     = m_config.get("pool_size").to_uint();

        expand(s);
    }
    
    ~MemoryPoolImpl() {            
        for ( auto &c : m_chunks )
            delete[] c.ptr;

        m_chunks.clear();
    }
};

// --- Static data initialization

const ConfigSet::Entry MemoryPool::MemoryPoolImpl::s_configdata[] = { 
    // key, type, value, short description, long description
    { "pool_size", CALI_TYPE_UINT, "2097152",
      "Initial size of the Caliper memory pool (in bytes)",
      "Initial size of the Caliper memory pool (in bytes)" 
    },
    { "can_expand", CALI_TYPE_BOOL, "true",
      "Allow memory pool to expand at runtime",
      "Allow memory pool to expand at runtime"
    },
    ConfigSet::Terminator
};


// --- MemoryPool public interface

MemoryPool::MemoryPool()
    : mP { new MemoryPoolImpl }
{ }

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
    return mP->allocate(bytes, mP->m_can_expand);
}

std::ostream& MemoryPool::print_statistics(std::ostream& os) const
{
    return mP->print_statistics(os);
}

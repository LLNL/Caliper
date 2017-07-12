// Copyright (c) 2016, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file  Trace.cpp
/// \brief Caliper trace service

#include "../CaliperService.h"

#include "TraceBufferChunk.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/util/spinlock.hpp"

#include <pthread.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_set>

using namespace trace;
using namespace cali;
using namespace std;

namespace 
{
    enum   BufferPolicy {
        Flush, Grow, Stop
    };
    
    struct TraceBuffer {
        std::atomic<bool>  stopped;
        std::atomic<bool>  retired;

        TraceBufferChunk*  chunks;
        TraceBuffer*       next;
        TraceBuffer*       prev;

        TraceBuffer(size_t s)
            : stopped(false), retired(false), chunks(new TraceBufferChunk(s)), next(0), prev(0)
            { }
        
        ~TraceBuffer() {
            delete chunks;
        }

        void unlink() {
            if (next)
                next->prev = prev;
            if (prev)
                prev->next = next;
        }
    };

    const ConfigSet::Entry configdata[] = {
        { "buffer_size",   CALI_TYPE_UINT, "2",
          "Size of initial per-thread trace buffer in MiB",
          "Size of initial per-thread trace buffer in MiB" },
        { "buffer_policy", CALI_TYPE_STRING, "grow",
          "What to do when trace buffer is full",
          "What to do when trace buffer is full:\n"
          "   flush:  Write out contents\n"
          "   grow:   Increase buffer size\n"
          "   stop:   Stop recording.\n"
          "Default: grow" },
        
        ConfigSet::Terminator
    };
    
    ConfigSet      config;
    
    BufferPolicy   policy            = BufferPolicy::Grow;
    size_t         buffersize        = 2 * 1024 * 1024;

    size_t         dropped_snapshots = 0;
    
    pthread_key_t  trace_buf_key;

    TraceBuffer*   global_tbuf_list  = nullptr;
    util::spinlock global_tbuf_lock;

    std::mutex     global_flush_lock;
    

    void destroy_tbuf(void* ctx) {
        TraceBuffer* tbuf = static_cast<TraceBuffer*>(ctx);

        if (!tbuf)
            return;

        tbuf->retired.store(true);
    }
    
    TraceBuffer* acquire_tbuf(bool alloc = true) {
        TraceBuffer* tbuf = static_cast<TraceBuffer*>(pthread_getspecific(trace_buf_key));

        if (alloc && !tbuf) {
            tbuf = new TraceBuffer(buffersize);

            if (!tbuf) {
                Log(0).stream() << "trace: error: unable to  allocate trace buffer!" << endl;
                return 0;
            }

            if (pthread_setspecific(trace_buf_key, tbuf) == 0) {
                std::lock_guard<util::spinlock>
                    g(global_tbuf_lock);

                if (global_tbuf_list)
                    global_tbuf_list->prev = tbuf;
                
                tbuf->next       = global_tbuf_list;
                global_tbuf_list = tbuf;                
            } else {
                Log(0).stream() << "trace: error: unable to set thread trace buffer" << endl;
                delete tbuf;
                tbuf = 0;
            }
        }

        return tbuf;
    }

    TraceBuffer* handle_overflow(Caliper* c, TraceBuffer* tbuf) {
        switch (policy) {
        case BufferPolicy::Stop:
            tbuf->stopped.store(true);
            Log(1).stream() << "Trace buffer full: recording stopped." << endl;
            return 0;
                
        case BufferPolicy::Grow:
        {
            TraceBufferChunk* newchunk = new TraceBufferChunk(buffersize);

            if (!newchunk) {
                Log(0).stream() << "trace: error: unable to allocate new trace buffer. Recording stopped." << endl;
                tbuf->stopped.store(true);
                return 0;
            }
            
            newchunk->append(tbuf->chunks);
            tbuf->chunks = newchunk;

            return tbuf;
        }
            
        case BufferPolicy::Flush:
        {
            Log(1).stream() << "Trace buffer full: flushing." << std::endl;

            c->flush_and_write(nullptr);
            
            return tbuf;
        }
        
        } // switch (policy)

        return 0;
    }
    
    void process_snapshot_cb(Caliper* c, const SnapshotRecord*, const SnapshotRecord* sbuf) {
        TraceBuffer* tbuf = acquire_tbuf(!c->is_signal());

        if (!tbuf || tbuf->stopped.load()) { // error messaging is done in acquire_tbuf()
            ++dropped_snapshots;
            return;
        }
        
        if (!tbuf->chunks->fits(sbuf))
            tbuf = handle_overflow(c, tbuf);
        if (!tbuf)
            return;

        tbuf->chunks->save_snapshot(sbuf);
    }        

    void flush_cb(Caliper* c, const SnapshotRecord*, Caliper::SnapshotProcessFn proc_fn) {
        std::lock_guard<std::mutex>
            g(global_flush_lock);

        TraceBuffer* tbuf = nullptr;
        
        {
            std::lock_guard<util::spinlock>
                g(global_tbuf_lock);
            
            tbuf = global_tbuf_list;
        }

        size_t num_written = 0;

        TraceBufferChunk::UsageInfo aggregate_info { 0, 0, 0 };
        
        while (tbuf) {
            // Stop tracing while we flush: writers won't block
            // but just drop the snapshot
            
            tbuf->stopped.store(true);

            // Accumulate usage statistics before they're reset in flush
            if (Log::verbosity() > 1) {
                TraceBufferChunk::UsageInfo info = tbuf->chunks->info();

                aggregate_info.nchunks  += info.nchunks;
                aggregate_info.reserved += info.reserved;
                aggregate_info.used     += info.used;
            }
            
            num_written += tbuf->chunks->flush(c, proc_fn);
            tbuf->stopped.store(false);
            
            if (tbuf->retired.load()) {
                // delete retired thread's trace buffer                
                TraceBuffer* tmp = tbuf->next;

                {
                    std::lock_guard<util::spinlock>
                        g(global_tbuf_lock);
                
                    tbuf->unlink();
                }
                
                delete tbuf;
                tbuf = tmp;
            } else {
                tbuf = tbuf->next;
            }
        }

        if (Log::verbosity() > 1) {
            Log(2).stream() << "Trace: "
                            << aggregate_info.reserved << " bytes reserved, "
                            << aggregate_info.used     << " bytes used in "
                            << aggregate_info.nchunks  << " chunks." << std::endl;
        }
        
        Log(1).stream() << "Trace: Flushed " << num_written << " snapshots." << endl;
    }

    void init_overflow_policy() {
        const map<std::string, BufferPolicy> polmap {
            { "grow",    BufferPolicy::Grow    },
            { "flush",   BufferPolicy::Flush   },
            { "stop",    BufferPolicy::Stop    } };

        string polname = config.get("buffer_policy").to_string();
        auto it = polmap.find(polname);

        if (it != polmap.end())
            policy = it->second;
        else
            Log(0).stream() << "trace: error: unknown buffer policy \"" << polname << "\"" << endl;
    }

    void create_scope_cb(Caliper* c, cali_context_scope_t scope) {
        // init trace buffer on new threads
        if (scope == CALI_SCOPE_THREAD)
            acquire_tbuf(true);
    }

    void finish_cb(Caliper* c) {
        if (dropped_snapshots > 0)
            Log(1).stream() << "Trace: dropped " << dropped_snapshots << " snapshots." << endl;
    }
    
    void trace_register(Caliper* c) {
        global_tbuf_lock.unlock();
        global_flush_lock.unlock();
        
        config = RuntimeConfig::init("trace", configdata);
        dropped_snapshots = 0;
        
        init_overflow_policy();
        
        buffersize = config.get("buffer_size").to_uint() * 1024 * 1024;
        
        if (pthread_key_create(&trace_buf_key, destroy_tbuf) != 0) {
            Log(0).stream() << "trace: error: pthread_key_create() failed" << endl;
            return;
        }        
        
        c->events().create_scope_evt.connect(&create_scope_cb);
        c->events().process_snapshot.connect(&process_snapshot_cb);
        c->events().flush_evt.connect(&flush_cb);
        c->events().finish_evt.connect(&finish_cb);

        // Initialize trace buffer on master thread
        acquire_tbuf(true);
        
        Log(1).stream() << "Registered trace service" << endl;
    }
    
} // namespace

namespace cali
{
    CaliperService trace_service { "trace", &::trace_register };
}

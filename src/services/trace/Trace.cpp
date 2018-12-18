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

// Trace.cpp
// Caliper trace service

#include "caliper/CaliperService.h"

#include "TraceBufferChunk.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/common/c-util/unitfmt.h"

#include "caliper/common/util/spinlock.hpp"

#include <atomic>
#include <cstring>
#include <mutex>

using namespace trace;
using namespace cali;

namespace 
{

class Trace
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

    class ThreadData {
        std::vector<TraceBuffer*> chn_buffers;

        //   The TraceBuffer objects are managed through the tbuf_list in the 
        // Trace object for the channel they belong to. Here, we store
        // pointers to the currently active trace buffers for all channels
        // on the local thread.
        //   Channels and their TraceBuffer objects can be deleted behind
        // the back of the thread-local data objects. Therefore, we keep track
        // of active channels in s_active_chns so we don't touch stale
        // TraceBuffer pointers whose channels have been deleted.
        
        static std::vector<bool>  s_active_chns;
        static std::mutex         s_active_chns_lock;

    public:

        ~ThreadData() {
            std::lock_guard<std::mutex>
                g(s_active_chns_lock);

            for (size_t i = 0; i < std::min<size_t>(chn_buffers.size(), s_active_chns.size()); ++i)
                if (s_active_chns[i] && chn_buffers[i])
                    chn_buffers[i]->retired.store(true);
        }

        static void deactivate_chn(Channel* chn) {
            std::lock_guard<std::mutex>
                g(s_active_chns_lock);
            
            s_active_chns[chn->id()] = false;
        }

        TraceBuffer* acquire_tbuf(Trace* trace, Channel* chn, bool alloc) {
            size_t       chnI = chn->id(); 
            TraceBuffer* tbuf = nullptr;
            
            if (chnI < chn_buffers.size())
                tbuf = chn_buffers[chnI];

            if (!tbuf && alloc) {
                tbuf = new TraceBuffer(trace->buffersize);

                if (chn_buffers.size() <= chnI)
                    chn_buffers.resize(std::max<size_t>(16, chnI+1));

                chn_buffers[chnI] = tbuf;

                {
                    std::lock_guard<std::mutex>
                        g(s_active_chns_lock);

                    if (s_active_chns.size() <= chnI)
                        s_active_chns.resize(std::max<size_t>(16, chnI+1));

                    s_active_chns[chnI] = true;
                }

                std::lock_guard<util::spinlock>
                    g(trace->tbuf_lock);

                if (trace->tbuf_list)
                    trace->tbuf_list->prev = tbuf;

                tbuf->next       = trace->tbuf_list;
                trace->tbuf_list = tbuf;
            }

            return tbuf;
        }
    };

    static const ConfigSet::Entry    s_configdata[];

    static thread_local ThreadData   sT;
    
    BufferPolicy   policy            = BufferPolicy::Grow;
    size_t         buffersize        = 2 * 1024 * 1024;

    size_t         dropped_snapshots = 0;    

    TraceBuffer*   tbuf_list  = nullptr;
    util::spinlock tbuf_lock;

    std::mutex     flush_lock;

    
    TraceBuffer* handle_overflow(Caliper* c, Channel* chn, TraceBuffer* tbuf) {
        switch (policy) {
        case BufferPolicy::Stop:
            tbuf->stopped.store(true);
            Log(1).stream() << chn->name() << ": Trace buffer full: recording stopped." << std::endl;
            return 0;
                
        case BufferPolicy::Grow:
        {
            TraceBufferChunk* newchunk = new TraceBufferChunk(buffersize);

            if (!newchunk) {
                Log(0).stream() << "Trace: error: unable to allocate new trace buffer. Recording stopped." << std::endl;
                tbuf->stopped.store(true);
                return 0;
            }
            
            newchunk->append(tbuf->chunks);
            tbuf->chunks = newchunk;

            return tbuf;
        }
            
        case BufferPolicy::Flush:
        {
            Log(1).stream() << chn->name() << ": Trace buffer full: flushing." << std::endl;

            c->flush_and_write(chn, nullptr);
            
            return tbuf;
        }
        
        } // switch (policy)

        return 0;
    }
    
    void process_snapshot_cb(Caliper* c, Channel* chn, const SnapshotRecord*, const SnapshotRecord* sbuf) {
        TraceBuffer* tbuf = sT.acquire_tbuf(this, chn, !c->is_signal());

        if (!tbuf || tbuf->stopped.load()) {
            ++dropped_snapshots;
            return;
        }
        
        if (!tbuf->chunks->fits(sbuf))
            tbuf = handle_overflow(c, chn, tbuf);
        if (!tbuf)
            return;

        tbuf->chunks->save_snapshot(sbuf);
    }        

    void flush_cb(Caliper* c, Channel* chn, const SnapshotRecord*, SnapshotFlushFn proc_fn) {
        std::lock_guard<std::mutex>
            g(flush_lock);

        TraceBuffer* tbuf = nullptr;
        
        {
            std::lock_guard<util::spinlock>
                g(tbuf_lock);
            
            tbuf = tbuf_list;
        }

        size_t num_written = 0;

        TraceBufferChunk::UsageInfo aggregate_info { 0, 0, 0 };
        
        for (; tbuf; tbuf = tbuf->next) {
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
        }

        if (Log::verbosity() > 1) {
            unitfmt_result bytes_reserved 
                = unitfmt(aggregate_info.reserved, unitfmt_bytes);
            unitfmt_result bytes_used     
                = unitfmt(aggregate_info.used,     unitfmt_bytes);

            Log(2).stream() << chn->name()             << ": Trace: "
                            << bytes_reserved.val      << " " 
                            << bytes_reserved.symbol   << " reserved, "
                            << bytes_used.val          << " " 
                            << bytes_used.symbol       << " used, "
                            << aggregate_info.nchunks  << " chunks." << std::endl;
        }
        
        Log(1).stream() << chn->name() << ": Trace: Flushed " << num_written << " snapshots." << std::endl;
    }

    void clear_cb(Caliper* c, Channel*) {
        std::lock_guard<std::mutex>
            g(flush_lock);

        TraceBuffer* tbuf = nullptr;
        
        {
            std::lock_guard<util::spinlock>
                g(tbuf_lock);
            
            tbuf = tbuf_list;
        }

        while (tbuf) {
            tbuf->stopped.store(true);
            tbuf->chunks->reset();
            tbuf->stopped.store(false);

            if (tbuf->retired.load()) {
                // delete retired thread's trace buffer                
                TraceBuffer* tmp = tbuf->next;

                {
                    std::lock_guard<util::spinlock>
                        g(tbuf_lock);
                
                    tbuf->unlink();

                    if (tbuf == tbuf_list)
                        tbuf_list = tmp;
                }
                
                delete tbuf;
                tbuf = tmp;
            } else {
                tbuf = tbuf->next;
            }
        }        
    }

    void init_overflow_policy(const std::string& polname) {
        const std::map<std::string, BufferPolicy> polmap {
            { "grow",    BufferPolicy::Grow    },
            { "flush",   BufferPolicy::Flush   },
            { "stop",    BufferPolicy::Stop    } };
        
        auto it = polmap.find(polname);

        if (it != polmap.end())
            policy = it->second;
        else
            Log(0).stream() << "Trace: error: unknown buffer policy \"" << polname << "\"" << std::endl;
    }

    void create_thread_cb(Caliper* c, Channel* chn) {
        // init trace buffer on new threads
        sT.acquire_tbuf(this, chn, true);
    }

    void finish_cb(Caliper* c, Channel* chn) {
        if (dropped_snapshots > 0)
            Log(1).stream() << chn->name() << ": Trace: dropped "
                            << dropped_snapshots << " snapshots." << std::endl;
    }

    Trace(Channel* chn)
        : dropped_snapshots(0)
        {
            tbuf_lock.unlock();
            flush_lock.unlock();
            
            ConfigSet cfg = chn->config().init("trace", s_configdata);
                    
            init_overflow_policy(cfg.get("buffer_policy").to_string());
            buffersize = cfg.get("buffer_size").to_uint() * 1024 * 1024;
        }
    
public:

    ~Trace()
        {
            // clear all trace buffers
            for (TraceBuffer* tbuf = tbuf_list; tbuf; tbuf = tbuf->next)
                delete tbuf;

            tbuf_list = nullptr;
        }
    
    static void trace_register(Caliper* c, Channel* chn) {
        Trace* instance = new Trace(chn);
        
        chn->events().create_thread_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->create_thread_cb(c, chn);
            });
        chn->events().process_snapshot.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* trigger, const SnapshotRecord* snapshot){
                instance->process_snapshot_cb(c, chn, trigger, snapshot);
            });
        chn->events().flush_evt.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* trigger, SnapshotFlushFn fn){
                instance->flush_cb(c, chn, trigger, fn);
            });
        chn->events().clear_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->clear_cb(c, chn);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                sT.deactivate_chn(chn);
                instance->finish_cb(c, chn);
                delete instance;
            });

        // Initialize trace buffer on master thread
        sT.acquire_tbuf(instance, chn, true);
        
        Log(1).stream() << chn->name() << ": Registered trace service" << std::endl;
    }
}; // class Trace

const ConfigSet::Entry Trace::s_configdata[] = {
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

thread_local Trace::ThreadData Trace::sT;

std::vector<bool> Trace::ThreadData::s_active_chns;
std::mutex        Trace::ThreadData::s_active_chns_lock;

} // namespace

namespace cali
{

CaliperService trace_service { "trace", ::Trace::trace_register };

}

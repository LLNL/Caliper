// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Trace.cpp
// Caliper trace service

#include "../Services.h"

#include "TraceBufferChunk.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"

#include "../../common/util/spinlock.hpp"
#include "../../common/util/unitfmt.h"

#include <atomic>
#include <cstring>
#include <mutex>

using namespace trace;
using namespace cali;

namespace
{

class Trace
{
    enum BufferPolicy { Flush, Grow, Stop };

    struct TraceBuffer {
        std::atomic<bool> stopped;
        std::atomic<bool> retired;

        TraceBufferChunk* chunks;
        TraceBuffer*      next;
        TraceBuffer*      prev;

        TraceBuffer(size_t s) : stopped(false), retired(false), chunks(new TraceBufferChunk(s)), next(0), prev(0) {}

        ~TraceBuffer() { delete chunks; }

        void unlink()
        {
            if (next)
                next->prev = prev;
            if (prev)
                prev->next = next;
        }
    };

    BufferPolicy policy     = BufferPolicy::Grow;
    size_t       buffersize = 2 * 1024 * 1024;

    size_t dropped_snapshots = 0;

    unsigned num_acquired = 0;
    unsigned num_released = 0;
    unsigned num_retired  = 0;

    Attribute tbuf_attr;

    TraceBuffer*   tbuf_list = nullptr;
    util::spinlock tbuf_lock;

    std::mutex flush_lock;

    Channel    m_channel;

    TraceBuffer* acquire_tbuf(Caliper* c, bool can_alloc)
    {
        //   we store a pointer to the thread-local trace buffer for this channel
        // on the thread's blackboard

        TraceBuffer* tbuf = static_cast<TraceBuffer*>(c->get(tbuf_attr).value().get_ptr());

        if (!tbuf && can_alloc) {
            tbuf = new TraceBuffer(buffersize);

            c->set(tbuf_attr, Variant(cali_make_variant_from_ptr(tbuf)));

            std::lock_guard<util::spinlock> g(tbuf_lock);

            if (tbuf_list)
                tbuf_list->prev = tbuf;

            tbuf->next = tbuf_list;
            tbuf_list  = tbuf;

            ++num_acquired;
        }

        return tbuf;
    }

    TraceBuffer* handle_overflow(Caliper* c, TraceBuffer* tbuf)
    {
        switch (policy) {
        case BufferPolicy::Stop:
            tbuf->stopped.store(true);
            Log(1).stream() << m_channel.name() << ": trace: Trace buffer full, recording stopped!\n";
            return 0;

        case BufferPolicy::Grow:
            {
                TraceBufferChunk* newchunk = new TraceBufferChunk(buffersize);

                if (!newchunk) {
                    Log(0).stream() << m_channel.name() << ": trace: Unable to allocate new trace buffer, recording stopped!\n";
                    tbuf->stopped.store(true);
                    return 0;
                }

                newchunk->append(tbuf->chunks);
                tbuf->chunks = newchunk;

                return tbuf;
            }

        case BufferPolicy::Flush:
            {
                Log(1).stream() << m_channel.name() << ": trace: Trace buffer full, flushing.\n";
                c->flush_and_write(m_channel.body(), SnapshotView());
                return tbuf;
            }

        } // switch (policy)

        return 0;
    }

    void process_snapshot_cb(Caliper* c, SnapshotView rec)
    {
        TraceBuffer* tbuf = acquire_tbuf(c, !c->is_signal());

        if (!tbuf || tbuf->stopped.load()) {
            ++dropped_snapshots;
            return;
        }

        if (!tbuf->chunks->fits(rec))
            tbuf = handle_overflow(c, tbuf);
        if (!tbuf)
            return;

        tbuf->chunks->save_snapshot(rec);
    }

    void flush_cb(Caliper* c, SnapshotFlushFn proc_fn)
    {
        std::lock_guard<std::mutex> g(flush_lock);
        TraceBuffer* tbuf = nullptr;

        {
            std::lock_guard<util::spinlock> g(tbuf_lock);
            tbuf = tbuf_list;
        }

        size_t num_written = 0;

        for (; tbuf; tbuf = tbuf->next) {
            // Stop tracing while we flush: writers won't block
            // but just drop the snapshot

            tbuf->stopped.store(true);
            num_written += tbuf->chunks->flush(c, proc_fn);
            tbuf->stopped.store(false);
        }

        Log(1).stream() << m_channel.name() << ": trace: Flushed " << num_written << " snapshots.\n";
    }

    void clear_cb(Caliper* c, Channel* chn)
    {
        std::lock_guard<std::mutex> g(flush_lock);
        TraceBuffer* tbuf = nullptr;

        {
            std::lock_guard<util::spinlock> g(tbuf_lock);
            tbuf = tbuf_list;
        }

        TraceBufferChunk::UsageInfo aggregate_info { 0, 0, 0 };

        while (tbuf) {
            tbuf->stopped.store(true);

            // Accumulate usage statistics before they're reset
            TraceBufferChunk::UsageInfo info = tbuf->chunks->info();

            aggregate_info.nchunks += info.nchunks;
            aggregate_info.reserved += info.reserved;
            aggregate_info.used += info.used;

            tbuf->chunks->reset();

            tbuf->stopped.store(false);

            if (tbuf->retired.load()) {
                // delete retired thread's trace buffer
                TraceBuffer* tmp = tbuf->next;

                {
                    std::lock_guard<util::spinlock> g(tbuf_lock);

                    tbuf->unlink();

                    if (tbuf == tbuf_list)
                        tbuf_list = tmp;

                    ++num_released;
                }

                delete tbuf;
                tbuf = tmp;
            } else {
                tbuf = tbuf->next;
            }
        }

        if (Log::verbosity() > 1) {
            unitfmt_result bytes_reserved = unitfmt(aggregate_info.reserved, unitfmt_bytes);
            unitfmt_result bytes_used     = unitfmt(aggregate_info.used, unitfmt_bytes);

            Log(2).stream() << chn->name() << ": trace: " << bytes_reserved.val << " " << bytes_reserved.symbol
                            << " reserved, " << bytes_used.val << " " << bytes_used.symbol << " used, "
                            << aggregate_info.nchunks << " chunks." << std::endl;
        }
    }

    void init_overflow_policy(const std::string& polname)
    {
        const std::map<std::string, BufferPolicy> polmap { { "grow", BufferPolicy::Grow },
                                                           { "flush", BufferPolicy::Flush },
                                                           { "stop", BufferPolicy::Stop } };

        auto it = polmap.find(polname);

        if (it != polmap.end())
            policy = it->second;
        else
            Log(0).stream() << "trace: error: unknown buffer policy \"" << polname << "\"" << std::endl;
    }

    void create_thread_cb(Caliper* c, Channel* chn)
    {
        // init trace buffer on new threads
        acquire_tbuf(c, true);
    }

    void release_thread_cb(Caliper* c, Channel* chn)
    {
        TraceBuffer* tbuf = acquire_tbuf(c, false);

        if (tbuf) {
            tbuf->retired.store(true);
            std::lock_guard<util::spinlock> g(tbuf_lock);
            ++num_retired;
        }
    }

    void finish_cb(Caliper* c, Channel* chn)
    {
        if (dropped_snapshots > 0)
            Log(1).stream() << chn->name() << ": Trace: dropped " << dropped_snapshots << " snapshots." << std::endl;
        if (Log::verbosity() >= 2)
            Log(2).stream() << chn->name() << ": Trace: " << num_acquired << " thread trace buffers acquired, "
                            << num_retired << " retired, " << num_released << " released." << std::endl;
    }

    Trace(Caliper* c, Channel* channel) : dropped_snapshots(0), m_channel { *channel }
    {
        ConfigSet cfg = services::init_config_from_spec(channel->config(), s_spec);

        init_overflow_policy(cfg.get("buffer_policy").to_string());
        buffersize = cfg.get("buffer_size").to_uint() * 1024 * 1024;

        tbuf_attr = c->create_attribute(
            std::string("trace.tbuf.") + std::to_string(channel->id()),
            CALI_TYPE_PTR,
            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS | CALI_ATTR_HIDDEN
        );
    }

public:

    static const char* s_spec;

    ~Trace()
    {
        // clear all trace buffers
        for (TraceBuffer *tbuf = tbuf_list, *tmp = nullptr; tbuf; tbuf = tmp) {
            tmp = tbuf->next;
            delete tbuf;
        }

        tbuf_list = nullptr;
    }

    static void trace_register(Caliper* c, Channel* chn)
    {
        Trace* instance = new Trace(c, chn);

        chn->events().create_thread_evt.connect([instance](Caliper* c, Channel* chn) {
            instance->create_thread_cb(c, chn);
        });
        chn->events().release_thread_evt.connect([instance](Caliper* c, Channel* chn) {
            instance->release_thread_cb(c, chn);
        });
        chn->events().process_snapshot.connect([instance](Caliper* c, SnapshotView, SnapshotView rec) {
            instance->process_snapshot_cb(c, rec);
        });
        chn->events().flush_evt.connect([instance](Caliper* c, SnapshotView, SnapshotFlushFn fn) {
            instance->flush_cb(c, fn);
        });
        chn->events().clear_evt.connect([instance](Caliper* c, Channel* chn) { instance->clear_cb(c, chn); });
        chn->events().finish_evt.connect([instance](Caliper* c, Channel* chn) {
            // sT.deactivate_chn(chn);
            instance->clear_cb(c, chn);
            instance->finish_cb(c, chn);
            delete instance;
        });

        // Initialize trace buffer on master thread
        instance->acquire_tbuf(c, true);

        Log(1).stream() << chn->name() << ": Registered trace service" << std::endl;
    }
}; // class Trace

const char* Trace::s_spec = R"json(
{
"name": "trace",
"description": "Store snapshots in trace buffer",
"config":
[
 {
  "name": "buffer_size",
  "description": "Size of initial per-thread trace buffer in MiB",
  "type": "uint",
  "value": "2"
 },{
  "name": "buffer_policy",
  "description": "What to do when the buffer is full ('flush', 'stop', 'grow')",
  "type": "string",
  "value": "grow"
 }
]}
)json";

} // namespace

namespace cali
{

CaliperService trace_service { ::Trace::s_spec, ::Trace::trace_register };

}

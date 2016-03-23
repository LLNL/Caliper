// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
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

/// @file  Trace.cpp
/// @brief Caliper trace service

#include "../CaliperService.h"

#include <Caliper.h>
#include <EntryList.h>

#include <ContextRecord.h>
#include <Log.h>
#include <Node.h>
#include <RuntimeConfig.h>

#include <util/spinlock.hpp>
#include <c-util/vlenc.h>

#include <pthread.h>

#include <cstring>
#include <mutex>
#include <unordered_set>

#define SNAP_MAX 64

using namespace cali;
using namespace std;

namespace 
{
    enum   BufferPolicy {
        Flush, Grow, Stop
    };
    
    class TraceBuffer {
        size_t         m_size;
        size_t         m_pos;
        size_t         m_nrec;

        bool           m_stop;
        
        unsigned char* m_data;
        
        TraceBuffer*   m_next;
        
    public:
        
        TraceBuffer(size_t s)
            : m_size(s), m_pos(0), m_nrec(0), m_stop(false), m_data(new unsigned char[s]), m_next(0)
            { }
        
        ~TraceBuffer() {
            delete[] m_data;

            if (m_next)
                delete m_next;
        }

        bool stopped() const { return m_stop; }
        void stop() {
            m_stop = true;
        }
        
        void append(TraceBuffer* tbuf) {
            m_next = tbuf;
        }

        void reset() {
            m_pos  = 0;
            m_nrec = 0;
            
            m_stop = false;

            memset(m_data, 0, m_size);
        }
        
        size_t flush(Caliper* c, unordered_set<cali_id_t>& written_node_cache) {
            size_t written = 0;

            //
            // local flush
            //

            size_t p = 0;

            for (size_t r = 0; r < m_nrec; ++r) {
                // decode snapshot record
                
                int n_nodes = static_cast<int>(std::min(static_cast<int>(vldec_u64(m_data + p, &p)), SNAP_MAX));
                int n_attr  = static_cast<int>(std::min(static_cast<int>(vldec_u64(m_data + p, &p)), SNAP_MAX));

                Variant node_vec[SNAP_MAX];
                Variant attr_vec[SNAP_MAX];
                Variant vals_vec[SNAP_MAX];

                for (int i = 0; i < n_nodes; ++i)
                    node_vec[i] = Variant(static_cast<cali_id_t>(vldec_u64(m_data + p, &p)));
                for (int i = 0; i < n_attr;  ++i)
                    attr_vec[i] = Variant(static_cast<cali_id_t>(vldec_u64(m_data + p, &p)));
                for (int i = 0; i < n_attr;  ++i)
                    vals_vec[i] = Variant::unpack(m_data + p, &p, nullptr);

                // write nodes
                // FIXME: this node cache is a terrible kludge, needs to go away
                //   either make node-by-id lookup fast,
                //   or fix node-before-snapshot I/O requirement

                for (int i = 0; i < n_nodes; ++i) {
                    cali_id_t node_id = node_vec[i].to_id();

                    if (written_node_cache.count(node_id))
                        continue;
                    
                    Node* node = c->node(node_vec[i].to_id());
                    
                    if (node)
                        node->write_path(c->events().write_record);

                    written_node_cache.insert(node_id);
                }
                for (int i = 0; i < n_attr; ++i) {
                    cali_id_t node_id = attr_vec[i].to_id();

                    if (written_node_cache.count(node_id))
                        continue;

                    Node* node = c->node(attr_vec[i].to_id());
                    
                    if (node)
                        node->write_path(c->events().write_record);

                    written_node_cache.insert(node_id);
                }

                // write snapshot
                
                int               n[3] = {  n_nodes,   n_attr,   n_attr };
                const Variant* data[3] = { node_vec, attr_vec, vals_vec };

                c->events().write_record(ContextRecord::record_descriptor(), n, data);
            }

            written += m_nrec;            
            reset();
            
            //
            // flush subsequent buffers in list
            // 
            
            if (m_next) {
                written += m_next->flush(c, written_node_cache);
                delete m_next;
                m_next = 0;
            }
            
            return written;
        }

        void save_snapshot(const EntryList* s) {
            EntryList::Sizes sizes = s->size();

            if ((sizes.n_nodes + sizes.n_immediate) == 0)
                return;

            sizes.n_nodes     = std::min<size_t>(sizes.n_nodes,     SNAP_MAX);
            sizes.n_immediate = std::min<size_t>(sizes.n_immediate, SNAP_MAX);
                
            m_pos += vlenc_u64(sizes.n_nodes,     m_data + m_pos);
            m_pos += vlenc_u64(sizes.n_immediate, m_data + m_pos);

            EntryList::Data addr = s->data();

            for (int i = 0; i < sizes.n_nodes; ++i)
                m_pos += vlenc_u64(addr.node_entries[i]->id(), m_data + m_pos);
            for (int i = 0; i < sizes.n_immediate;  ++i)
                m_pos += vlenc_u64(addr.immediate_attr[i],     m_data + m_pos);
            for (int i = 0; i < sizes.n_immediate;  ++i)
                m_pos += addr.immediate_data[i].pack(m_data + m_pos);

            ++m_nrec;
        }
        
        bool fits(const EntryList* s) const {
            EntryList::Sizes sizes = s->size();

            // get worst-case estimate of packed snapshot size:
            //   20 bytes for size indicators
            //   10 bytes per node id
            //   10+22 bytes per immediate entry (10 for attr, 22 for variant)
            
            size_t max = 20 + 10 * sizes.n_nodes + 32 * sizes.n_immediate;

            return (m_pos + max) < m_size;
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
    
    BufferPolicy   policy           = BufferPolicy::Grow;
    size_t         buffersize       = 2 * 1024 * 1024;
    
    pthread_key_t  trace_buf_key;

    TraceBuffer*   global_tbuf_list = nullptr;
    util::spinlock global_tbuf_lock;

    
    void save_tbuf(TraceBuffer* tb) {
        pthread_setspecific(trace_buf_key, tb);
    }

    void destroy_tbuf(void* ctx) {
        TraceBuffer* tbuf = static_cast<TraceBuffer*>(ctx);

        if (!tbuf)
            return;
        else {
            std::lock_guard<util::spinlock> g(global_tbuf_lock);
            
            tbuf->append(global_tbuf_list);
            global_tbuf_list = tbuf;
        }
    }
    
    TraceBuffer* acquire_tbuf() {
        TraceBuffer* tbuf = static_cast<TraceBuffer*>(pthread_getspecific(trace_buf_key));

        if (!tbuf) {
            tbuf = new TraceBuffer(buffersize);

            if (!tbuf) {
                Log(0).stream() << "trace: error: unable to  allocate trace buffer!" << endl;
                return 0;
            }

            if (pthread_setspecific(trace_buf_key, tbuf) != 0) {
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
            tbuf->stop();
            Log(1).stream() << "Trace buffer full: recording stopped." << endl;
            return 0;
                
        case BufferPolicy::Grow:
        {
            TraceBuffer* newtbuf = new TraceBuffer(buffersize);

            if (!newtbuf) {
                Log(0).stream() << "trace: error: unable to allocate new trace buffer. Recording stopped." << endl;
                tbuf->stop();
                return 0;
            }

            newtbuf->append(tbuf);
            tbuf = newtbuf;
            save_tbuf(tbuf);

            return tbuf;
        }
            
        case BufferPolicy::Flush:
        {
            unordered_set<cali_id_t> written_node_cache;
            Log(1).stream() << "Trace buffer full: flushed " << tbuf->flush(c, written_node_cache) << " snapshots." << endl;
            return tbuf;
        }
        
        } // switch (policy)

        return 0;
    }
    
    void process_snapshot_cb(Caliper* c, const EntryList*, const EntryList* sbuf) {
        TraceBuffer* tbuf = acquire_tbuf();

        if (!tbuf || tbuf->stopped()) // error messaging is done in acquire_tbuf()
            return;
        
        if (!tbuf->fits(sbuf))
            tbuf = handle_overflow(c, tbuf);
        if (!tbuf)
            return;

        tbuf->save_snapshot(sbuf);
    }        

    void flush_cb(Caliper* c, const EntryList*) {
        TraceBuffer* gtbuf = nullptr;
        
        {
            std::lock_guard<util::spinlock> g(global_tbuf_lock);
            
            gtbuf            = global_tbuf_list;
            global_tbuf_list = nullptr;
        }

        unordered_set<cali_id_t> written_node_cache;
            
        if (gtbuf) {
            Log(1).stream() << "Flushed " << gtbuf->flush(c, written_node_cache) << " snapshots." << endl;
            delete gtbuf;
        }
        
        TraceBuffer* tbuf = acquire_tbuf();

        if (tbuf)
            Log(1).stream() << "Flushed " << tbuf->flush(c, written_node_cache)  << " snapshots." << endl;
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
    
    void trace_register(Caliper* c) {
        global_tbuf_lock.unlock();
        
        config = RuntimeConfig::init("trace", configdata);
        
        init_overflow_policy();
        
        buffersize = config.get("buffer_size").to_uint() * 1024 * 1024;
        
        if (pthread_key_create(&trace_buf_key, destroy_tbuf) != 0) {
            Log(0).stream() << "trace: error: pthread_key_create() failed" << endl;
            return;
        }        
        
        c->events().process_snapshot.connect(&process_snapshot_cb);
        c->events().flush.connect(&flush_cb);

        Log(1).stream() << "Registered trace service" << endl;
    }
    
} // namespace

namespace cali
{
    CaliperService TraceService { "trace", &::trace_register };
}

/// @file Caliper.cpp
/// Caliper main class
///

#include "Caliper.h"
#include "ContextBuffer.h"
#include "MemoryPool.h"
#include "SigsafeRWLock.h"

#include <Services.h>

#include <AttributeStore.h>
#include <ContextRecord.h>
#include <MetadataWriter.h>
#include <Node.h>
#include <Log.h>
#include <RecordMap.h>
#include <RuntimeConfig.h>

#include <signal.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>
#include <utility>

using namespace cali;
using namespace std;


//
// --- Caliper implementation
//

struct Caliper::CaliperImpl
{
    // --- static data

    static volatile sig_atomic_t  s_siglock;
    static std::mutex             s_mutex;
    
    static unique_ptr<Caliper>    s_caliper;

    static const ConfigSet::Entry s_configdata[];

    // --- data

    ConfigSet              m_config;

    ContextBuffer*         m_default_process_context;
    ContextBuffer*         m_default_thread_context;
    ContextBuffer*         m_default_task_context;

    function<ContextBuffer*()>  m_get_thread_contextbuffer_cb;
    function<ContextBuffer*()>  m_get_task_contextbuffer_cb;
    
    MemoryPool             m_mempool;

    mutable SigsafeRWLock  m_nodelock;
    Node                   m_root;
    Node*                  m_last_written_node;
    atomic<unsigned>       m_node_id;

    mutable SigsafeRWLock  m_attribute_lock;
    map<string, Node*>     m_attribute_nodes;
    Node*                  m_type_nodes[CALI_MAXTYPE+1];

    Attribute              m_name_attr;
    Attribute              m_type_attr;
    Attribute              m_prop_attr;
 
    Events                 m_events;

    // --- constructor

    CaliperImpl()
        : m_config { RuntimeConfig::init("caliper", s_configdata) }, 
        m_default_process_context { new ContextBuffer },
        m_default_thread_context  { new ContextBuffer },
        m_default_task_context    { new ContextBuffer },
        m_root { CALI_INV_ID, CALI_INV_ID, { } }, 
        m_last_written_node { &m_root },
        m_node_id { 0 },
        m_name_attr { Attribute::invalid }, 
        m_type_attr { Attribute::invalid },  
        m_prop_attr { Attribute::invalid }
    { 
    }

    ~CaliperImpl() {
        m_events.finish_evt(s_caliper.get());

        Log(1).stream() << "Finished" << endl;

        for ( auto &n : m_root )
            n.~Node();

        // prevent re-initialization
        s_siglock = 2;
    }

    // deferred initialization: called when it's safe to use the public Caliper interface

    void 
    init() {
        bootstrap();

        Services::register_services(s_caliper.get());

        Log(1).stream() << "Initialized" << endl;

        if (Log::verbosity() >= 2)
            RuntimeConfig::print( Log(2).stream() << "Configuration:\n" );
    }

    void  
    bootstrap() {
        // Create initial nodes

        static Node bootstrap_type_nodes[] = {
            {  0, 9, { CALI_TYPE_USR    },  },
            {  1, 9, { CALI_TYPE_INT    },  },
            {  2, 9, { CALI_TYPE_UINT   },  },
            {  3, 9, { CALI_TYPE_STRING },  },
            {  4, 9, { CALI_TYPE_ADDR   },  },
            {  5, 9, { CALI_TYPE_DOUBLE },  },
            {  6, 9, { CALI_TYPE_BOOL   },  },
            {  7, 9, { CALI_TYPE_TYPE   },  },
            { CALI_INV_ID, CALI_INV_ID, { } } 
        };
        static Node bootstrap_attr_nodes[] = {
            {  8, 8, { CALI_TYPE_STRING, "cali.attribute.name", 19 } },
            {  9, 8, { CALI_TYPE_STRING, "cali.attribute.type", 19 } },
            { 10, 8, { CALI_TYPE_STRING, "cali.attribute.prop", 19 } },
            { CALI_INV_ID, CALI_INV_ID, { } } 
        };

        for ( Node* nodes : { bootstrap_type_nodes, bootstrap_attr_nodes } )
            for (Node* node = nodes ; node->id() != CALI_INV_ID; ++node) {
                m_node_id.store(static_cast<unsigned>(node->id() + 1));

                m_last_written_node->list().insert(node);
                m_last_written_node = node;
            }

        // Fill type map

        for (Node* node = bootstrap_type_nodes ; node->id() != CALI_INV_ID; ++node)
            m_type_nodes[node->data().to_attr_type()] = node;

        // Initialize bootstrap attributes

        struct attr_node_t { 
            Node* node; Attribute* attr; cali_attr_type type;
        } attr_nodes[] = { 
            { &bootstrap_attr_nodes[0], &m_name_attr, CALI_TYPE_STRING },
            { &bootstrap_attr_nodes[1], &m_type_attr, CALI_TYPE_TYPE   },
            { &bootstrap_attr_nodes[2], &m_prop_attr, CALI_TYPE_INT    } 
        };

        for ( attr_node_t p : attr_nodes ) {
            // Create attribute 
            *(p.attr) = Attribute(p.node->id(), p.node->data().to_string(), p.type);
            // Append to type node
            m_type_nodes[p.type]->append(p.node);
        }
    }

    // --- helpers

    cali_context_scope_t 
    get_scope(const Attribute& attr) const {
        const struct scope_tbl_t {
            cali_attr_properties attrscope; cali_context_scope_t ctxscope;
        } scope_tbl[] = {
            { CALI_ATTR_SCOPE_THREAD,  CALI_SCOPE_THREAD  },
            { CALI_ATTR_SCOPE_PROCESS, CALI_SCOPE_PROCESS },
            { CALI_ATTR_SCOPE_TASK,    CALI_SCOPE_TASK    }
        };

        for (scope_tbl_t s : scope_tbl)
            if ((attr.properties() & CALI_ATTR_SCOPE_MASK) == s.attrscope)
                return s.ctxscope;

        // make thread scope the default
        return CALI_SCOPE_THREAD;
    }

    Attribute 
    make_attribute(const Node* node) const {
        Variant   name, type, prop;
        cali_id_t id = node ? node->id() : CALI_INV_ID;

        for ( ; node ; node = node->parent() ) {
            if      (node->attribute() == m_name_attr.id()) 
                name = node->data();
            else if (node->attribute() == m_prop_attr.id()) 
                prop = node->data();
            else if (node->attribute() == m_type_attr.id()) 
                type = node->data();
        }

        if (!name || !type)
            return Attribute::invalid;

        int p = prop ? prop.to_int() : CALI_ATTR_DEFAULT;

        return Attribute(id, name.to_string(), type.to_attr_type(), p);
    }


    /// @brief Creates @param n new nodes hierarchically under @param parent 

    Node*
    create_path(size_t n, const Attribute* attr, const Variant* data, Node* parent = nullptr) {
        // Calculate and allocate required memory

        const size_t align = 8;
        const size_t pad   = align - sizeof(Node)%align;

        size_t total_size  = 0;

        for (size_t i = 0; i < n; ++i) {
            total_size += sizeof(Node) + pad;

            if (attr[i].type() == CALI_TYPE_USR || attr[i].type() == CALI_TYPE_STRING)
                total_size += data[i].size() + (align - data[i].size()%align);
        }

        char* ptr  = static_cast<char*>(m_mempool.allocate(total_size));
        Node* node = nullptr;

        // Create nodes

        for (size_t i = 0; i < n; ++i) {
            bool   copy { attr[i].type() == CALI_TYPE_USR || attr[i].type() == CALI_TYPE_STRING };
            const void* dptr { data[i].data() };
            size_t size { data[i].size() }; 

            if (copy)
                dptr = memcpy(ptr+sizeof(Node)+pad, dptr, size);

            node = new(ptr) 
                Node(m_node_id.fetch_add(1), attr[i].id(), Variant(attr[i].type(), dptr, size));

            m_nodelock.wlock();

            if (parent)
                parent->append(node);

            m_last_written_node->list().insert(node);
            m_last_written_node = node;

            m_nodelock.unlock();

            ptr   += sizeof(Node)+pad + (copy ? size+(align-size%align) : 0);
            parent = node;
        }

        return node;
    }

    /// @brief Retreive the given node hierarchy under @param parent
    /// Creates new nodes if necessery

    Node*
    get_path(size_t n, const Attribute* attr, const Variant* data, Node* parent = nullptr) {
        Node*  node = parent ? parent : &m_root;
        size_t base = 0;

        for (size_t i = 0; i < n; ++i) {
            parent = node;

            m_nodelock.rlock();
            for (node = parent->first_child(); node && !node->equals(attr[i].id(), data[i]); node = node->next_sibling())
                ;
            m_nodelock.unlock();

            if (!node)
                break;

            ++base;
        }

        if (!node)
            node = create_path(n-base, attr+base, data+base, parent);

        return node;
    }

    // --- Environment interface

    ContextBuffer*
    default_contextbuffer(cali_context_scope_t scope) const {
        switch (scope) {
        case CALI_SCOPE_PROCESS:
            return m_default_process_context;
        case CALI_SCOPE_THREAD:
            return m_default_thread_context;
        case CALI_SCOPE_TASK:
            return m_default_task_context;
        }

        assert(!"Unknown context scope type!");

        return nullptr;
    }

    ContextBuffer*
    current_contextbuffer(cali_context_scope_t scope) {
        switch (scope) {
        case CALI_SCOPE_PROCESS:
            // Currently, there is only one process environment which always gets ID 0
            return m_default_process_context;
        case CALI_SCOPE_THREAD:
            if (m_get_thread_contextbuffer_cb)
                return m_get_thread_contextbuffer_cb();
            break;
        case CALI_SCOPE_TASK:
            if (m_get_task_contextbuffer_cb)
                return m_get_task_contextbuffer_cb();
            break;
        }

        return default_contextbuffer(scope);
    }

    void 
    set_contextbuffer_callback(cali_context_scope_t scope, std::function<ContextBuffer*()> cb) {
        switch (scope) {
        case CALI_SCOPE_THREAD:
            if (m_get_thread_contextbuffer_cb)
                Log(0).stream() 
                    << "Caliper::set_context_callback(): error: callback already set" 
                    << endl;
            m_get_thread_contextbuffer_cb = cb;
            break;
        case CALI_SCOPE_TASK:
            if (m_get_task_contextbuffer_cb)
                Log(0).stream() 
                    << "Caliper::set_context_callback(): error: callback already set" 
                    << endl;
            m_get_task_contextbuffer_cb = cb;
            break;
        default:
            Log(0).stream() 
                << "Caliper::set_context_callback(): error: cannot set process callback" 
                << endl;
        }
    }

    ContextBuffer*
    create_contextbuffer(cali_context_scope_t scope) {
        ContextBuffer* ctx = new ContextBuffer;
        m_events.create_context_evt(scope, ctx);

        return ctx;
    }

    void
    release_contextbuffer(ContextBuffer* ctx) {
        m_events.destroy_context_evt(ctx);
        delete ctx;
    }

    // --- Attribute interface

    Attribute 
    create_attribute(const std::string& name, cali_attr_type type, int prop) {
        // Add default SCOPE_THREAD property if no other is set
        if (!(prop & CALI_ATTR_SCOPE_PROCESS) && !(prop & CALI_ATTR_SCOPE_TASK))
            prop |= CALI_ATTR_SCOPE_THREAD;

        Node* node { nullptr };

        // Check if an attribute with this name already exists

        m_attribute_lock.rlock();

        auto it = m_attribute_nodes.find(name);
        if (it != m_attribute_nodes.end())
            node = it->second;

        m_attribute_lock.unlock();

        // Create attribute nodes

        if (!node) {
            assert(type >= 0 && type <= CALI_MAXTYPE);
            Node*     type_node = m_type_nodes[type];
            assert(type_node);

            Attribute attr[2] { m_prop_attr, m_name_attr };
            Variant   data[2] { { prop }, { CALI_TYPE_STRING, name.c_str(), name.size() } };

            if (prop == CALI_ATTR_DEFAULT)
                node = get_path(1, &attr[1], &data[1], type_node);
            else
                node = get_path(2, &attr[0], &data[0], type_node);

            if (node) {
                // Check again if attribute already exists; might have been created by 
                // another thread in the meantime.
                // We've created some redundant nodes then, but that's fine
                m_attribute_lock.wlock();

                auto it = m_attribute_nodes.lower_bound(name);

                if (it == m_attribute_nodes.end() || it->first != name)
                    m_attribute_nodes.emplace_hint(it, name, node);
                else
                    node = it->second;

                m_attribute_lock.unlock();
            }
        }

        // Create attribute object

        Attribute attr { make_attribute(node) };

        m_events.create_attr_evt(s_caliper.get(), attr);

        return attr;
    }

    Attribute
    get_attribute(const string& name) const {
        Node* node = nullptr;

        m_attribute_lock.rlock();

        auto it = m_attribute_nodes.find(name);

        if (it != m_attribute_nodes.end())
            node = it->second;

        m_attribute_lock.unlock();

        return make_attribute(node);
    }

    Attribute 
    get_attribute(cali_id_t id) const {
        return make_attribute(get(id));
    }

    size_t
    num_attributes() const {
        m_attribute_lock.rlock();
        size_t size = m_attribute_nodes.size();
        m_attribute_lock.unlock();

        return size;
    }

    // --- Context interface

    std::size_t 
    pull_context(int scope, uint64_t buf[], std::size_t len) {
        // Trigger query event: allows services to update context information

        m_events.query_evt(s_caliper.get(), scope);

        // TODO: run measure() to receive explicit measurements from services

        // Pull context from current TASK/THREAD/PROCESS environments

        ContextBuffer* ctxbuf[3] { nullptr, nullptr, nullptr };
        int            n         { 0 };

        if (scope & CALI_SCOPE_TASK)
            ctxbuf[n++] = current_contextbuffer(CALI_SCOPE_TASK);
        if (scope & CALI_SCOPE_THREAD)
            ctxbuf[n++] = current_contextbuffer(CALI_SCOPE_THREAD);
        if (scope & CALI_SCOPE_PROCESS)
            ctxbuf[n++] = current_contextbuffer(CALI_SCOPE_PROCESS);

        size_t clen = 0;

        for (int e = 0; e < n && ctxbuf[e]; ++e)
            clen += ctxbuf[e]->pull_context(buf+clen, len - std::min(clen, len));

        return clen;
    }

    void
    push_context(int scope) {
        // Trigger query event: allows services to update context information

        m_events.query_evt(s_caliper.get(), scope);

        const int MAX_DATA  = 40;

        int        all_n[3] = { 0, 0, 0 };
        Variant all_data[3][MAX_DATA];

        // Coalesce selected context buffer and measurement records into a single record

        auto coalesce_rec = [&](const RecordDescriptor& rec, const int* n, const Variant** data){
            assert(rec.id == ContextRecord::record_descriptor().id && rec.num_entries == 3);

            for (int i : { 0, 1, 2 }) {
                for (int v = 0; v < n[i] && all_n[i]+v < MAX_DATA; ++v)
                    all_data[i][all_n[i]+v] = data[i][v];

                all_n[i] = min(all_n[i]+n[i], MAX_DATA);
            }
        };

        m_events.measure(scope, coalesce_rec);

        for (cali_context_scope_t s : { CALI_SCOPE_TASK, CALI_SCOPE_THREAD, CALI_SCOPE_PROCESS })
            if (scope & s)
                current_contextbuffer(s)->push_record(coalesce_rec);

        const Variant* all_data_p[3] = { all_data[0], all_data[1], all_data[2] };

        // Write any nodes that haven't been written 

        m_nodelock.wlock();

        for (Node* node; (node = m_root.list().next()) != 0; node->list().unlink())
            node->push_record(m_events.write_record);

        m_last_written_node = &m_root;

        m_nodelock.unlock();

        // Write context record

        m_events.write_record(ContextRecord::record_descriptor(), all_n, all_data_p);
    }

    // --- Annotation interface

    cali_err 
    begin(const Attribute& attr, const Variant& data) {
        cali_err ret = CALI_EINV;

        if (attr == Attribute::invalid)
            return CALI_EINV;

        // invoke callbacks
        if (!attr.skip_events())
            m_events.pre_begin_evt(s_caliper.get(), attr);

        ContextBuffer* ctx = current_contextbuffer(get_scope(attr));

        if (attr.store_as_value())
            ret = ctx->set(attr, data);
        else
            ret = ctx->set_node(attr, get_path(1, &attr, &data, ctx->get_node(attr)));

        // invoke callbacks
        if (!attr.skip_events())
            m_events.post_begin_evt(s_caliper.get(), attr);

        return ret;
    }

    cali_err 
    end(const Attribute& attr) {
        if (attr == Attribute::invalid)
            return CALI_EINV;

        cali_err ret = CALI_EINV;
        ContextBuffer* ctx = current_contextbuffer(get_scope(attr));

        // invoke callbacks
        if (!attr.skip_events())
            m_events.pre_end_evt(s_caliper.get(), attr);

        if (attr.store_as_value())
            ret = ctx->unset(attr);
        else {
            Node* node = ctx->get_node(attr);

            if (!node)
                return CALI_EINV;

            m_nodelock.rlock();

            if (node->attribute() != attr.id()) {
                // For now, just continue before first node with this attribute
                while (node && node->attribute() != attr.id())
                    node = node->parent();

                if (!node)
                    return CALI_EINV;
            }

            node = node->parent();
            m_nodelock.unlock();

            if (node == &m_root)
                ret = ctx->unset(attr);
            else if (node)
                ret = ctx->set_node(attr, node);
        }

        // invoke callbacks
        if (!attr.skip_events())
            m_events.post_end_evt(s_caliper.get(), attr);

        return ret;
    }

    cali_err 
    set(const Attribute& attr, const Variant& data) {
        cali_err ret = CALI_EINV;

        if (attr == Attribute::invalid)
            return CALI_EINV;

        ContextBuffer* ctx = current_contextbuffer(get_scope(attr));

        // invoke callbacks
        if (!attr.skip_events())
            m_events.pre_set_evt(s_caliper.get(), attr);

        if (attr.store_as_value()) {
            ret = ctx->set(attr, data);
        } else {
            Node* parent = ctx->get_node(attr);

            if (parent)
                parent = parent->parent();

            ret = ctx->set_node(attr, get_path(1, &attr, &data, parent));
        }

        // invoke callbacks
        if (!attr.skip_events())
            m_events.post_set_evt(s_caliper.get(), attr);

        return ret;
    }

    // --- Retrieval

    const Node* 
    get(cali_id_t id) const {
        const Node* ret = nullptr;

        m_nodelock.rlock();

        for (const Node* typenode : m_type_nodes)
            for (auto &n : *typenode)
                if (n.id() == id) {
                    ret = &n;
                    break;
                }

        if (!ret)
            for (auto &n : m_root)
                if (n.id() == id) {
                    ret = &n;
                    break;
                }

        m_nodelock.unlock();

        return ret;
    }


    // --- Serialization API

    void 
    foreach_node(std::function<void(const Node&)> proc) {
        // Need locking?
        for (auto &n : m_root)
            if (n.id() != CALI_INV_ID)
                proc(n);
    }
};


// --- static member initialization

volatile sig_atomic_t  Caliper::CaliperImpl::s_siglock = 1;
mutex                  Caliper::CaliperImpl::s_mutex;

unique_ptr<Caliper>    Caliper::CaliperImpl::s_caliper;

const ConfigSet::Entry Caliper::CaliperImpl::s_configdata[] = {
    // key, type, value, short description, long description
    { "node_pool_size", CALI_TYPE_UINT, "100",
      "Size of the Caliper node pool",
      "Initial size of the Caliper node pool" 
    },
    { "output", CALI_TYPE_STRING, "csv",
      "Caliper metadata output format",
      "Caliper metadata output format. One of\n"
      "   csv:  CSV file output\n"
      "   none: No output" 
    },
    ConfigSet::Terminator 
};


//
// --- Caliper class definition
//

Caliper::Caliper()
    : mP(new CaliperImpl)
{ 
}

Caliper::~Caliper()
{
    mP.reset(nullptr);
}

// --- Events interface

Caliper::Events&
Caliper::events()
{
    return mP->m_events;
}


// --- Context environment API

ContextBuffer*
Caliper::default_contextbuffer(cali_context_scope_t scope) const
{
    return mP->default_contextbuffer(scope);
}

ContextBuffer*
Caliper::current_contextbuffer(cali_context_scope_t scope)
{
    return mP->current_contextbuffer(scope);
}

ContextBuffer*
Caliper::create_contextbuffer(cali_context_scope_t scope)
{
    return mP->create_contextbuffer(scope);
}

void
Caliper::release_contextbuffer(ContextBuffer* ctxbuf)
{
    mP->release_contextbuffer(ctxbuf);
}

void 
Caliper::set_contextbuffer_callback(cali_context_scope_t scope, std::function<ContextBuffer*()> cb)
{
    mP->set_contextbuffer_callback(scope, cb);
}

// --- Context API

std::size_t 
Caliper::context_size(int scope) const
{
    // return mP->m_context.context_size(env);
    return 2 * num_attributes();
}

std::size_t 
Caliper::pull_context(int scope, uint64_t buf[], std::size_t len) 
{
    return mP->pull_context(scope, buf, len);
}

void 
Caliper::push_context(int scope) 
{
    return mP->push_context(scope);
}

// --- Annotation interface

cali_err 
Caliper::begin(const Attribute& attr, const Variant& data)
{
    return mP->begin(attr, data);
}

cali_err 
Caliper::end(const Attribute& attr)
{
    return mP->end(attr);
}

cali_err 
Caliper::set(const Attribute& attr, const Variant& data)
{
    return mP->set(attr, data);
}


// --- Attribute API

size_t
Caliper::num_attributes() const                       { return mP->num_attributes();    }
Attribute
Caliper::get_attribute(cali_id_t id) const            { return mP->get_attribute(id);   }
Attribute
Caliper::get_attribute(const std::string& name) const { return mP->get_attribute(name); }

Attribute 
Caliper::create_attribute(const std::string& name, cali_attr_type type, int prop)
{
    return mP->create_attribute(name, type, prop);
}


// --- Caliper query API

// std::vector<RecordMap>
// Caliper::unpack(const uint64_t buf[], size_t size) const
// {
//     return ContextRecord::unpack(
//         [this](cali_id_t id){ return mP->get_attribute(id); },
//         [this](cali_id_t id){ return mP->get(id); },
//         buf, size);                                 
// }


// --- Serialization API

void
Caliper::foreach_node(std::function<void(const Node&)> proc)
{
    mP->foreach_node(proc);
}

// --- Caliper singleton API

Caliper* Caliper::instance()
{
    if (CaliperImpl::s_siglock != 0) {
        if (CaliperImpl::s_siglock == 2)
            // Caliper had been initialized previously; we're past the static destructor
            return nullptr;

        lock_guard<mutex> lock(CaliperImpl::s_mutex);

        if (!CaliperImpl::s_caliper) {
            CaliperImpl::s_caliper.reset(new Caliper);

            // now it is safe to use the Caliper interface
            CaliperImpl::s_caliper->mP->init();

            CaliperImpl::s_siglock = 0;
        }
    }

    return CaliperImpl::s_caliper.get();
}

Caliper* Caliper::try_instance()
{
    return CaliperImpl::s_siglock == 0 ? CaliperImpl::s_caliper.get() : nullptr;
}

/// @file Caliper.cpp
/// Caliper main class
///

#include "Caliper.h"
#include "Context.h"
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
    
    cali_id_t              m_default_thread_env;
    cali_id_t              m_default_task_env;

    function<cali_id_t()>  m_get_thread_env_cb;
    function<cali_id_t()>  m_get_task_env_cb;
    
    MemoryPool             m_mempool;

    mutable SigsafeRWLock  m_nodelock;
    vector<Node*>          m_nodes;
    Node                   m_root;

    mutable SigsafeRWLock  m_attribute_lock;
    map<string, Node*>
                           m_attribute_nodes;
    map<cali_attr_type, Node*> 
                           m_type_nodes;

    Attribute              m_name_attr;
    Attribute              m_type_attr;
    Attribute              m_prop_attr;
 
    Context                m_context;

    Events                 m_events;

    // --- constructor

    CaliperImpl()
        : m_config { RuntimeConfig::init("caliper", s_configdata) }, 
        m_default_thread_env { CALI_INV_ID },
        m_default_task_env   { CALI_INV_ID },
        m_root { CALI_INV_ID, CALI_INV_ID, { } }, 
        m_name_attr { Attribute::invalid }, 
        m_type_attr { Attribute::invalid },  
        m_prop_attr { Attribute::invalid }
    { 
    }

    ~CaliperImpl() {
        Log(1).stream() << "Finished" << endl;

        for ( auto &n : m_nodes )
            n->~Node();
    }

    // deferred initialization: called when it's safe to use the public Caliper interface

    void 
    init() {
        m_nodes.reserve(m_config.get("node_pool_size").to_uint());

        bootstrap();

        Services::register_services(s_caliper.get());

        // Create default environments

        m_default_thread_env = m_context.create_environment();
        m_default_task_env   = m_context.create_environment();

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

        m_nodes.resize(11);

        for ( Node* nodes : { bootstrap_type_nodes, bootstrap_attr_nodes } )
            for (Node* node = nodes ; node->id() != CALI_INV_ID; ++node)
                m_nodes[node->id()] = node;

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
        if (attr.properties()      & CALI_ATTR_SCOPE_PROCESS)
            return CALI_SCOPE_PROCESS;
        else if (attr.properties() & CALI_ATTR_SCOPE_TASK)
            return CALI_SCOPE_TASK;

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

    Node*
    create_path(size_t n, const Attribute* attr, Variant* data, Node* parent = nullptr) {
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

        m_nodelock.wlock();

        for (size_t i = 0; i < n; ++i) {
            bool   copy { attr[i].type() == CALI_TYPE_USR || attr[i].type() == CALI_TYPE_STRING };
            const void* dptr { data[i].data() };
            size_t size { data[i].size() }; 

            if (copy)
                dptr = memcpy(ptr+sizeof(Node)+pad, dptr, size);

            node = new(ptr) 
                Node(m_nodes.size(), attr[i].id(), Variant(attr[i].type(), dptr, size));

            m_nodes.push_back(node);

            if (parent)
                parent->append(node);

            ptr   += sizeof(Node)+pad + (copy ? size+(align-size%align) : 0);
            parent = node;
        }

        m_nodelock.unlock();

        return node;
    }

    Node* 
    create_node(const Attribute& attr, const void* data, size_t size, Node* parent = nullptr) {
        Variant v { attr.type(), data, size };

        return create_path(1, &attr, &v, parent);
    }


    // --- Environment interface

    cali_id_t
    default_environment(cali_context_scope_t scope) const {
        switch (scope) {
        case CALI_SCOPE_PROCESS:
            return 0;
        case CALI_SCOPE_THREAD:
            assert(m_default_thread_env != CALI_INV_ID);
            return m_default_thread_env;
        case CALI_SCOPE_TASK:
            assert(m_default_task_env != CALI_INV_ID);
            return m_default_task_env;
        }

        assert(!"Unknown context scope type!");

        return CALI_INV_ID;
    }

    cali_id_t
    current_environment(cali_context_scope_t scope) {
        switch (scope) {
        case CALI_SCOPE_PROCESS:
            // Currently, there is only one process environment which always gets ID 0
            return 0;
        case CALI_SCOPE_THREAD:
            if (m_get_thread_env_cb)
                return m_get_thread_env_cb();
        case CALI_SCOPE_TASK:
            if (m_get_task_env_cb)
                return m_get_task_env_cb();
        }

        return default_environment(scope);
    }

    void 
    set_environment_callback(cali_context_scope_t scope, std::function<cali_id_t()> cb) {
        switch (scope) {
        case CALI_SCOPE_THREAD:
            if (m_get_thread_env_cb)
                Log(0).stream() 
                    << "Caliper::set_environment_callback(): error: callback already set" 
                    << endl;
            m_get_thread_env_cb = cb;
            break;
        case CALI_SCOPE_TASK:
            if (m_get_task_env_cb)
                Log(0).stream() 
                    << "Caliper::set_environment_callback(): error: callback already set" 
                    << endl;
            m_get_task_env_cb = cb;
            break;
        default:
            Log(0).stream() 
                << "Caliper::set_environment_callback(): error: cannot set process callback" 
                << endl;
        }
    }


    // --- Attribute interface

    Attribute 
    create_attribute(const std::string& name, cali_attr_type type, int prop) {
        // Add default SCOPE_THREAD property if no other is set
        if (!(prop & CALI_ATTR_SCOPE_PROCESS) && !(prop & CALI_ATTR_SCOPE_TASK))
            prop |= CALI_ATTR_SCOPE_THREAD;

        m_attribute_lock.wlock();

        Node* node { nullptr };
        auto it = m_attribute_nodes.find(name);

        if (it != m_attribute_nodes.end()) {
            node = it->second;
        } else {
            Node*     type_node = m_type_nodes[type];

            assert(type_node);

            Attribute attr[2] { m_prop_attr, m_name_attr };
            Variant   data[2] { { prop }, { CALI_TYPE_STRING, name.c_str(), name.size() } };

            if (prop == CALI_ATTR_DEFAULT)
                node = create_path(1, &attr[1], &data[1], type_node);
            else
                node = create_path(2, &attr[0], &data[0], type_node);

            if (node)
                m_attribute_nodes.insert(make_pair(name, node));
        }

        m_attribute_lock.unlock();

        return make_attribute(node);
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
        m_nodelock.rlock();
        Node* node = id < m_nodes.size() ? m_nodes[id] : nullptr;
        m_nodelock.unlock();

        return make_attribute(node);
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
    get_context(cali_context_scope_t scope, uint64_t buf[], std::size_t len) {
        // invoke callbacks
        m_events.queryEvt(s_caliper.get(), scope);

        // collect context from current TASK/THREAD/PROCESS environments

        cali_id_t envs[3] = { 0, 0, 0 };
        int       nenvs = 0;

        switch (scope) {
        case CALI_SCOPE_TASK:
            envs[nenvs++] = current_environment(CALI_SCOPE_TASK);
        case CALI_SCOPE_THREAD:
            envs[nenvs++] = current_environment(CALI_SCOPE_THREAD);
        case CALI_SCOPE_PROCESS:
            envs[nenvs++] = current_environment(CALI_SCOPE_PROCESS);
        }

        size_t clen = 0;

        for (int e = 0; e < nenvs; ++e)
            clen += m_context.get_context(envs[e], buf+clen, len - std::min(clen, len));

        return clen;
    }


    // --- Annotation interface

    cali_err 
    begin(const Attribute& attr, const void* data, size_t size) {
        cali_err ret = CALI_EINV;

        if (attr == Attribute::invalid)
            return CALI_EINV;

        cali_id_t env = current_environment(get_scope(attr));
        cali_id_t key = attr.id();

        if (attr.store_as_value() && size == sizeof(uint64_t)) {
            uint64_t val = 0;
            memcpy(&val, data, sizeof(uint64_t));
            ret = m_context.set(env, key, val);
        } else {
            auto p = m_context.get(env, key);

            m_nodelock.rlock();

            Node* parent = p.first ? m_nodes[p.second] : &m_root;
            Node* node   = parent ? parent->first_child() : nullptr;

            while ( node && !node->equals(attr.id(), data, size) )
                node = node->next_sibling();

            m_nodelock.unlock();

            if (!node)
                node = create_node(attr, data, size, parent);

            ret = m_context.set(env, key, node->id());
        }

        // invoke callbacks
        m_events.beginEvt(s_caliper.get(), env, attr);

        return ret;
    }

    cali_err 
    end(const Attribute& attr) {
        if (attr == Attribute::invalid)
            return CALI_EINV;

        cali_err  ret = CALI_EINV;

        cali_id_t env = current_environment(get_scope(attr));
        cali_id_t key = attr.id();

        if (attr.store_as_value())
            ret = m_context.unset(env, key);
        else {
            auto p = m_context.get(env, key);

            if (!p.first)
                return CALI_EINV;

            m_nodelock.rlock();

            Node* node = m_nodes[p.second];

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
                ret = m_context.unset(env, key);
            else if (node)
                ret = m_context.set(env, key, node->id());
        }

        // invoke callbacks
        m_events.endEvt(s_caliper.get(), env, attr);

        return ret;
    }

    cali_err 
    set(const Attribute& attr, const void* data, size_t size) {
        if (attr == Attribute::invalid)
            return CALI_EINV;

        cali_err  ret = CALI_EINV;

        cali_id_t env = current_environment(get_scope(attr));
        cali_id_t key = attr.id();

        if (attr.store_as_value() && size == sizeof(uint64_t)) {
            uint64_t val = 0;
            memcpy(&val, data, sizeof(uint64_t));
            ret = m_context.set(env, key, val);
        } else {
            auto p = m_context.get(env, key);

            Node* parent { nullptr };

            m_nodelock.rlock();

            if (p.first)
                parent = m_nodes[p.second]->parent();
            if (!parent)
                parent = &m_root;

            Node* node = parent->first_child();

            while ( node && !node->equals(attr.id(), data, size) )
                node = node->next_sibling();

            m_nodelock.unlock();

            if (!node)
                node = create_node(attr, data, size, parent);

            ret = m_context.set(env, key, node->id());
        }

        // invoke callbacks
        m_events.setEvt(s_caliper.get(), env, attr);

        return ret;
    }


    // --- Retrieval

    const Node* 
    get(cali_id_t id) const {
        if (id > m_nodes.size())
            return nullptr;

        const Node* ret = nullptr;

        m_nodelock.rlock();
        ret = m_nodes[id];
        m_nodelock.unlock();

        return ret;
    }


    // --- Serialization API

    void 
    foreach_node(std::function<void(const Node&)> proc) {
        // Need locking?
        for (Node* node : m_nodes)
            if (node)
                proc(*node);
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

cali_id_t
Caliper::default_environment(cali_context_scope_t scope) const
{
    return mP->default_environment(scope);
}

cali_id_t 
Caliper::current_environment(cali_context_scope_t scope)
{
    return mP->current_environment(scope);
}

cali_id_t
Caliper::create_environment()
{
    return mP->m_context.create_environment();
}

cali_id_t 
Caliper::clone_environment(cali_id_t env)
{
    return mP->m_context.clone_environment(env);
}

void
Caliper::release_environment(cali_id_t env)
{
    return mP->m_context.release_environment(env);
}

void 
Caliper::set_environment_callback(cali_context_scope_t scope, std::function<cali_id_t()> cb)
{
    mP->set_environment_callback(scope, cb);
}

// --- Context API

std::size_t 
Caliper::context_size(cali_context_scope_t scope) const
{
    // return mP->m_context.context_size(env);
    return 2 * num_attributes();
}

std::size_t 
Caliper::get_context(cali_context_scope_t scope, uint64_t buf[], std::size_t len) 
{
    return mP->get_context(scope, buf, len);
}


// --- Annotation interface

cali_err 
Caliper::begin(const Attribute& attr, const void* data, size_t size)
{
    return mP->begin(attr, data, size);
}

cali_err 
Caliper::end(const Attribute& attr)
{
    return mP->end(attr);
}

cali_err 
Caliper::set(const Attribute& attr, const void* data, size_t size)
{
    return mP->set(attr, data, size);
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

std::vector<RecordMap>
Caliper::unpack(const uint64_t buf[], size_t size) const
{
    return ContextRecord::unpack(
        [this](cali_id_t id){ return mP->get_attribute(id); },
        [this](cali_id_t id){ return mP->get(id); },
        buf, size);                                 
}


// --- Serialization API

void
Caliper::foreach_node(std::function<void(const Node&)> proc)
{
    mP->foreach_node(proc);
}

bool
Caliper::write_metadata()
{    
    string writer_service_name = mP->m_config.get("output").to_string();

    if (writer_service_name == "none")
        return true;

    auto w = Services::get_metadata_writer(writer_service_name.c_str());

    if (!w) {
        Log(0).stream() << "Writer service \"" << writer_service_name << "\" not found!" << endl;
        return false;
    }

    using std::placeholders::_1;

    return w->write(std::bind(&CaliperImpl::foreach_node, mP.get(), _1));
}


// --- Caliper singleton API

Caliper* Caliper::instance()
{
    if (CaliperImpl::s_siglock == 1) {
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

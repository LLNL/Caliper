/// @file Caliper.cpp
/// Caliper main class
///

#include "Caliper.h"

#include "AttributeStore.h"
#include "Context.h"
#include "MemoryPool.h"
#include "Node.h"

#include <signal.h>

#include <cstring>
#include <map>
#include <mutex>
#include <vector>

using namespace cali;
using namespace std;


//
// --- static data initialization --------------------------------------------
//

const  std::size_t cali_node_pool_size = 100; // make this a config variable


//
// --- Caliper implementation
//

struct Caliper::CaliperImpl
{
    // --- static data

    static volatile sig_atomic_t s_siglock;
    static std::mutex            s_mutex;
    
    static unique_ptr<Caliper>   s_caliper;


    // --- data
    
    MemoryPool           m_mempool;

    vector<Node*>        m_nodes;
    Node                 m_root;

    AttributeStore       m_attributes;
    Context              m_context;


    // --- constructor

    CaliperImpl()
        : m_mempool { 2 * 1024 * 1024 }, 
        m_root { CTX_INV_ID, CTX_INV_ID, 0, 0 } 
    {
        m_nodes.reserve(cali_node_pool_size);
    }

    ~CaliperImpl() {
        for ( auto &n : m_nodes )
            n->~Node();
    }


    // --- helpers

    Node* create_node(ctx_id_t attr, const void* data, size_t size) {
        const size_t align = 8;
        const size_t pad   = align - sizeof(Node)%align;

        char* ptr  = static_cast<char*>(m_mempool.allocate(sizeof(Node) + pad + size));
        Node* node = new(ptr) 
            Node(m_nodes.size(), attr, memcpy(ptr+sizeof(Node)+pad, data, size), size);

        m_nodes.push_back(node);

        return node;
    }


    // --- interface

    ctx_err begin(ctx_id_t env, const Attribute& attr, const void* data, size_t size) {
        ctx_err ret = CTX_EINV;

        if (attr == Attribute::invalid)
            return CTX_EINV;

        ctx_id_t key = attr.id();

        if (attr.store_as_value() && size == sizeof(uint64_t)) {
            uint64_t val = 0;
            memcpy(&val, data, sizeof(uint64_t));
            ret = m_context.set(env, key, val, attr.is_global());
        } else {
            auto p = m_context.get(env, key);

            Node* parent = p.first ? m_nodes[p.second] : &m_root;
            Node* node   = parent ? parent->first_child() : nullptr;

            while ( node && !node->equals(attr.id(), data, size) )
                node = node->next_sibling();

            if (!node) {
                node = create_node(attr.id(), data, size);

                if (parent)
                    parent->append(node);
            }

            ret = m_context.set(env, key, node->id(), attr.is_global());
        }

        return ret;
    }

    ctx_err end(ctx_id_t env, const Attribute& attr) {
        if (attr == Attribute::invalid)
            return CTX_EINV;

        ctx_err  ret = CTX_EINV;
        ctx_id_t key = attr.id();

        if (attr.store_as_value())
            ret = m_context.unset(env, key);
        else {
            auto p = m_context.get(env, key);

            if (!p.first)
                return CTX_EINV;

            Node* node = m_nodes[p.second];

            if (node->attribute() != attr.id()) {
                // For now, just continue before first node with this attribute
                while (node && node->attribute() != attr.id())
                    node = node->parent();

                if (!node)
                    return CTX_EINV;
            }

            node = node->parent();

            if (node == &m_root)
                ret = m_context.unset(env, key);
            else if (node)
                ret = m_context.set(env, key, node->id());
        }

        return ret;
    }

    ctx_err set(ctx_id_t env, const Attribute& attr, const void* data, size_t size) {
        if (attr == Attribute::invalid)
            return CTX_EINV;

        ctx_err  ret = CTX_EINV;
        ctx_id_t key = attr.id();

        if (attr.store_as_value() && size == sizeof(uint64_t)) {
            uint64_t val = 0;
            memcpy(&val, data, sizeof(uint64_t));
            ret = m_context.set(env, key, val, attr.is_global());
        } else {
            auto p = m_context.get(env, key);

            Node* parent { nullptr };

            if (p.first)
                parent = m_nodes[p.second]->parent();
            if (!parent)
                parent = &m_root;

            Node* node = parent->first_child();

            while ( node && !node->equals(attr.id(), data, size) )
                node = node->next_sibling();

            if (!node) {
                node = create_node(attr.id(), data, size);

                if (parent)
                    parent->append(node);
            }

            ret = m_context.set(env, key, node->id(), attr.is_global());
        }

        return ret;
    }


    // --- Query API

    vector<QueryKey> unpack(const uint64_t buf[], size_t size) const {
        vector<QueryKey> vec;

        for (size_t i = 0; i < size / 2; ++i) {
            ctx_id_t attr = buf[2*i];
            uint64_t val  = buf[2*i+1];

            auto p = m_attributes.get(attr);

            if (!p.first) // Oops?! Shouldn't happen
                return vec;
            
            if (p.second.store_as_value())
                vec.push_back(QueryKey(p.second.id(), val));
            else {
                // unpack all nodes up to root
                while (val < m_nodes.size()) {
                    vec.push_back(QueryKey(attr, val));

                    Node* parent = m_nodes[val]->parent();

                    attr = parent ? parent->attribute() : CTX_INV_ID;
                    val  = parent ? parent->id() : CTX_INV_ID;
                }
            }
        }

        return vec;
    }

    class CaliperQuery : public Query {
        CaliperImpl* cI;
        Attribute    m_attr;
        uint64_t     m_value;

    public:

        CaliperQuery(CaliperImpl* c, const Attribute& attr, uint64_t val)
            : cI { c }, m_attr { attr }, m_value { val }
            { }

        bool          valid() const override     { return !(m_attr == Attribute::invalid); }

        std::string   attribute() const override { return m_attr.name(); }
        ctx_attr_type type() const override      { return m_attr.type(); }

        size_t        size() const override {
            return m_attr.store_as_value() ? sizeof(uint64_t) : cI->m_nodes[m_value]->size();
        }
        const void*   data() const override {
            return m_attr.store_as_value() ? &m_value : cI->m_nodes[m_value]->data();
        }
    };

    unique_ptr<cali::Query> query(const Attribute& attr, uint64_t val) {
        return unique_ptr<cali::Query> { new CaliperQuery(this, attr, val) };
    }
};


// --- static member initialization

volatile sig_atomic_t Caliper::CaliperImpl::s_siglock = 1;
mutex                 Caliper::CaliperImpl::s_mutex;
    
unique_ptr<Caliper>   Caliper::CaliperImpl::s_caliper;

Caliper::QueryKey     Caliper::QueryKey::invalid { CTX_INV_ID, 0 };


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

// --- Context API

ctx_id_t 
Caliper::current_environment() const
{
    return 0;
}

ctx_id_t 
Caliper::clone_environment(ctx_id_t env)
{
    return mP->m_context.clone_environment(env);
}

std::size_t 
Caliper::context_size(ctx_id_t env) const
{
    return mP->m_context.context_size(env);
}

std::size_t 
Caliper::get_context(ctx_id_t env, uint64_t buf[], std::size_t len) const
{
    return mP->m_context.get_context(env, buf, len);
}

ctx_err 
Caliper::begin(ctx_id_t env, const Attribute& attr, const void* data, size_t size)
{
    return mP->begin(env, attr, data, size);
}

ctx_err 
Caliper::end(ctx_id_t env, const Attribute& attr)
{
    return mP->end(env, attr);
}

ctx_err 
Caliper::set(ctx_id_t env, const Attribute& attr, const void* data, size_t size)
{
    return mP->set(env, attr, data, size);
}


// --- Attribute API

std::pair<bool, Attribute> 
Caliper::get_attribute(ctx_id_t id) const
{
    return mP->m_attributes.get(id);
}

std::pair<bool, Attribute> 
Caliper::get_attribute(const std::string& name) const
{
    return mP->m_attributes.get(name);
}

Attribute 
Caliper::create_attribute(const std::string& name, ctx_attr_type type, int prop)
{
    return mP->m_attributes.create(name, type, prop);
}


// --- Caliper query API

std::vector<Caliper::QueryKey> 
Caliper::unpack(const uint64_t buf[], size_t size) const
{
    return mP->unpack(buf, size);
}

std::unique_ptr<Query> 
Caliper::query(const QueryKey& key) const
{
    return mP->query(mP->m_attributes.get(key.m_attr).second, key.m_value);
}


// --- Caliper singleton API

Caliper* Caliper::instance()
{
    if (CaliperImpl::s_siglock == 1) {
        lock_guard<mutex> lock(CaliperImpl::s_mutex);

        if (!CaliperImpl::s_caliper) {
            CaliperImpl::s_caliper.reset(new Caliper);
            CaliperImpl::s_siglock = 0;
        }
    }

    return CaliperImpl::s_caliper.get();
}

Caliper* Caliper::try_instance()
{
    return CaliperImpl::s_siglock == 0 ? CaliperImpl::s_caliper.get() : nullptr;
}

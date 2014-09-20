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
    AttributeStore       m_attributes;

    Context              m_context;


    // --- constructor

    CaliperImpl()
        : m_mempool { 2 * 1024 * 1024 } { 
        m_nodes.reserve(cali_node_pool_size);
    }

    ~CaliperImpl() {
        for ( auto &n : m_nodes )
            n->~Node();
    }


    // --- helpers

    Node* create_node(ctx_id_t attr, void* data, size_t size) {
        const size_t align = 8;
        const size_t pad   = align - sizeof(Node)%align;

        char* ptr  = static_cast<char*>(m_mempool.allocate(sizeof(Node) + pad + size));
        Node* node = new(ptr) 
            Node(m_nodes.size(), attr, memcpy(ptr+sizeof(Node)+pad, data, size), size);

        m_nodes.push_back(node);

        return node;
    }


    // --- interface

    ctx_err begin(ctx_id_t env, const Attribute& attr, void* data, size_t size) {
        if (attr.store_as_value() && size <= sizeof(uint64_t)) {
            uint64_t val = 0;
            memcpy(&val, data, sizeof(uint64_t));
            m_context.set(env, attr.id(), val, attr.clone());
        } else {
            auto p = m_context.get(env, attr.id());

            Node* parent = p.first ? m_nodes[p.second] : nullptr;
            Node* node   = parent ? parent->first_child() : nullptr;

            for ( ; node && !node->equals(attr.id(), data, size); node = node->next_sibling())
                ;

            if (!node) {
                node = create_node(attr.id(), data, size);

                if (parent)
                    parent->append(node);
            }

            m_context.set(env, attr.id(), node->id(), attr.clone());
        }

        return CTX_SUCCESS;
    }

    ctx_err end(ctx_id_t env, const Attribute& attr) {
        if (attr.store_as_value())
            m_context.unset(env, attr.id());
        else {
            auto p = m_context.get(env, attr.id());

            if (!p.first)
                return CTX_EINV;

            Node* node = m_nodes[p.second];

            // For now, revert to parent of node with same attribute
            while (node && node->attribute() != attr.id())
                node = node->parent();

            if (node)
                m_context.set(env, node->attribute(), node->id());
            else
                m_context.unset(env, attr.id());
        }

        return CTX_SUCCESS;
    }

    ctx_err set(ctx_id_t env, const Attribute& attr, void* data, size_t size) {
        if (attr.store_as_value() && size <= sizeof(uint64_t)) {
            uint64_t val = 0;
            memcpy(&val, data, sizeof(uint64_t));
            m_context.set(env, attr.id(), val, attr.clone());
        } else {
            auto p = m_context.get(env, attr.id());

            Node* parent = p.first ? m_nodes[p.second]->parent() : nullptr;
            Node* node   = parent ? parent->first_child() : nullptr;

            for ( ; node && !node->equals(attr.id(), data, size); node = node->next_sibling())
                ;

            if (!node) {
                node = create_node(attr.id(), data, size);

                if (parent)
                    parent->append(node);
            }

            m_context.set(env, attr.id(), node->id(), attr.clone());
        }

        return CTX_SUCCESS;
    }
};


// --- static member initialization

volatile sig_atomic_t Caliper::CaliperImpl::s_siglock = 1;
mutex                 Caliper::CaliperImpl::s_mutex;
    
unique_ptr<Caliper>   Caliper::CaliperImpl::s_caliper;


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
Caliper::begin(ctx_id_t env, const Attribute& attr, void* data, size_t size)
{
    return mP->begin(env, attr, data, size);
}

ctx_err 
Caliper::end(ctx_id_t env, const Attribute& attr)
{
    return mP->end(env, attr);
}

ctx_err 
Caliper::set(ctx_id_t env, const Attribute& attr, void* data, size_t size)
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
Caliper::create_attribute(const std::string& name, ctx_attr_properties prop, ctx_attr_type type)
{
    return mP->m_attributes.create(name, prop, type);
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

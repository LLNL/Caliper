// @file init.cpp
// Initialization function and global data
//

#include "init.h"

#include <signal.h>

#include <mutex>
#include <set>

using namespace cali;
using namespace std;


//
// --- static data initialization --------------------------------------------
//

const  std::size_t cali_node_pool_size = 100; // make this a config variable


//
// --- Caliper implememtation
//

struct Caliper::CaliperImpl
{
    // --- static data

    static sig_atomic_t        s_siglock;
    static std::mutex          s_mutex;
    
    static unique_ptr<Caliper> s_caliper;


    // --- data

    map< size_t, NodePool >                  m_nodepool;

    map< ctx_id_t, shared_ptr<Attribute>   > m_attributes;
    map< ctx_id_t, shared_ptr<Environment> > m_environments;

    // --- constructor

    // --- interface

    void addNewAttribute(const Attribute& attr) {
        m_attributes.insert(attr);

        if (!attr->store_as_value()) {
            auto it = m_nodes.find(attr.typesize());

            if (it == m_nodes.end())
                it = m_nodes.insert( vector<shared_ptr<Node>> { } );

            if (it->second.size() < cali_node_pool_size) {
                it->second.reserve(it->second.capacity() + cali_node_pool_size);

                for (size_type i = 0; i < cali_node_pool_size; ++i)
                    it->second.emplace_back(make_shared<Node>(get_new_node_id()));
            }
        }
    }
}

// --- static member initialization

sig_atomic_t        Caliper::CaliperImpl::s_siglock = 1;
mutex               Caliper::CaliperImpl::s_mutex;
    
unique_ptr<Caliper> Caliper::CaliperImpl::s_caliper;


Caliper::Caliper()
    : mP(new CaliperImpl)
{ 
}


void Caliper::addNewAttribute(const Attribute& attr) 
{
    mP->addNewAttribute(attr);
}


static Caliper* Caliper::instance()
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

static Caliper* Caliper::try_instance()
{
    return CaliperImpl::s_siglock == 0 ? CaliperImpl::s_caliper.get() : nullptr;
}

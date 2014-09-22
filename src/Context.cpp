/// @file Context.cpp
/// Caliper Context class implementation

#include "Context.h"

#include <algorithm>
#include <utility>
#include <vector>


using namespace cali;
using namespace std;

struct Context::ContextImpl 
{
    // --- data 

    typedef vector< pair<ctx_id_t, uint64_t> > env_vec_t;

    vector<env_vec_t> m_environments;


    // --- constructor

    ContextImpl() {
        env_vec_t env;
        env.reserve(8); // FIXME: make configurable
        m_environments.push_back(move(env));
    }

    // --- interface

    ctx_id_t clone_environment(ctx_id_t env) {
        if (env == CTX_INV_ID || env >= m_environments.size())
            return CTX_INV_ID;

        m_environments.emplace_back( m_environments[env] );

        return m_environments.size();
    }

    void release_environment(ctx_id_t env) {
        m_environments.erase(m_environments.begin() + env);
    }

    size_t record_size(ctx_id_t env) const {
        return env < m_environments.size() ? m_environments[env].size() * 2 : 0;
    }

    size_t get_context(ctx_id_t env, uint64_t buf[], size_t len) const {
        if (!(env < m_environments.size()))
            return 0;

        auto env_p = m_environments.begin() + env;
        size_t s = 0;

        for (auto it = env_p->begin() ; (s + 1) < len && it != env_p->end(); ++it, s += 2) {
            buf[s]   = it->first;
            buf[s+1] = it->second;
        }

        return s;
    }

    pair<bool, uint64_t> get(ctx_id_t env, ctx_id_t key) const {
        if (!(env < m_environments.size()))
            return make_pair<bool, uint64_t>(false, 0);

        auto env_p = m_environments.begin() + env;
        auto it = lower_bound(env_p->begin(), env_p->end(), make_pair(key, uint64_t(0)));

        if (it == env_p->end() || it->first != key)
            return make_pair<bool, uint64_t>(false, 0);

        return make_pair(true, it->second);
    }

    void set (ctx_id_t env, ctx_id_t key, uint64_t value, bool /* clone */) {
        if (!(env < m_environments.size()))
            return;

        auto env_p = m_environments.begin() + env;
        auto it = lower_bound(env_p->begin(), env_p->end(), make_pair(key, uint64_t(0)));

        if (it != env_p->end() && it->first == key)
            it->second = value;
        else
            env_p->insert(it, make_pair(key, value));
    }

    void unset(ctx_id_t env, uint64_t key) {
        if (!(env < m_environments.size()))
            return;

        auto env_p = m_environments.begin() + env;
        auto it = lower_bound(env_p->begin(), env_p->end(), make_pair(key, uint64_t(0)));

        if (it != env_p->end() && it->first == key)
            env_p->erase(it);
    }
};

Context::Context()
    : mP(new ContextImpl)
{ 
}

Context::~Context()
{
    mP.reset();
}

ctx_id_t Context::clone_environment(ctx_id_t id)
{
    return mP->clone_environment(id);
}

void Context::release_environment(ctx_id_t env)
{
    return mP->release_environment(env);
}

size_t Context::context_size(ctx_id_t env) const 
{
    return mP->record_size(env);
}

size_t Context::get_context(ctx_id_t env, uint64_t buf[], size_t len) const
{
    return mP->get_context(env, buf, len);
}

pair<bool, uint64_t> Context::get(ctx_id_t env, ctx_id_t key) const
{
    return mP->get(env, key);
}

void Context::set(ctx_id_t env, ctx_id_t key, uint64_t value, bool clone)
{
    mP->set(env, key, value, clone);
}

void Context::unset(ctx_id_t env, ctx_id_t key)
{
    mP->unset(env, key);
}

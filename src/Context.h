/// @file context.h 
/// Caliper context environments

#ifndef CALI_CONTEXT_H
#define CALI_CONTEXT_H

#include "cali_types.h"

#include <memory>
#include <utility>

namespace cali
{

class Context 
{
    struct ContextImpl;

    std::unique_ptr<ContextImpl> mP;

public:

    Context();

    ~Context();

    ctx_id_t clone_environment(ctx_id_t env);
    void release_environment(ctx_id_t env);

    std::size_t context_size(ctx_id_t env) const;
    std::size_t get_context(ctx_id_t env, uint64_t buf[], std::size_t len) const;
    
    std::pair<bool, uint64_t> get(ctx_id_t env, ctx_id_t key) const;
    ctx_err set(ctx_id_t env, ctx_id_t key, uint64_t value, bool global = false);
    ctx_err unset(ctx_id_t env, ctx_id_t key);
};

} // namespace cali

#endif // CALI_CONTEXT_H

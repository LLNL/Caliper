/// @file context.h 
/// Caliper context environments

#ifndef CALI_CONTEXT_H
#define CALI_CONTEXT_H

#include <cali_types.h>

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

    cali_id_t clone_environment(cali_id_t env);
    cali_id_t create_environment();
    void release_environment(cali_id_t env);

    std::size_t context_size(cali_id_t env) const;
    std::size_t get_context(cali_id_t env, uint64_t buf[], std::size_t len) const;
    
    std::pair<bool, uint64_t> get(cali_id_t env, cali_id_t key) const;
    cali_err set(cali_id_t env, cali_id_t key, uint64_t value);
    cali_err unset(cali_id_t env, cali_id_t key);
};

} // namespace cali

#endif // CALI_CONTEXT_H

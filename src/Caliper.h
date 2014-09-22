/** 
 * @file caliper.h 
 * Initialization function and global data declaration
 */

#ifndef CALI_CALIPER_H
#define CALI_CALIPER_H

#include "cali_types.h"

#include "Attribute.h"

#include <memory>
#include <utility>

namespace cali
{

// Forward declarations

class Caliper 
{
    struct CaliperImpl;

    std::unique_ptr<CaliperImpl> mP;


    Caliper();

public:

    ~Caliper();

    Caliper(const Caliper&) = delete;

    Caliper& operator = (const Caliper&) = delete;


    // --- Context API

    ctx_id_t current_environment() const;

    ctx_id_t clone_environment(ctx_id_t env);

    std::size_t context_size(ctx_id_t env) const;
    std::size_t get_context(ctx_id_t env, uint64_t buf[], std::size_t len) const;

    ctx_err begin(ctx_id_t env, const Attribute& attr, const void* data, size_t size);
    ctx_err end(ctx_id_t env, const Attribute& attr);
    ctx_err set(ctx_id_t env, const Attribute& attr, const void* data, size_t size);


    // --- Attribute API

    std::pair<bool, Attribute> get_attribute(ctx_id_t id) const;
    std::pair<bool, Attribute> get_attribute(const std::string& name) const;

    Attribute create_attribute(const std::string& name, ctx_attr_properties prop, ctx_attr_type type);


    // --- Caliper singleton API

    static Caliper* instance();
    static Caliper* try_instance();
};

} // namespace cali

#endif // CALI_CALIPER_H

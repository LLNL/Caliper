/** 
 * @file caliper.h 
 * Initialization function and global data declaration
 */

#ifndef CALI_CALIPER_H
#define CALI_CALIPER_H

#include <Attribute.h>
#include <RecordMap.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>


namespace cali
{

// Forward declarations

class Node;


/// @class Caliper

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

    void set_environment_callback(std::function<ctx_id_t()> cb);


    // --- Annotation API

    ctx_err begin(ctx_id_t env, const Attribute& attr, const void* data, std::size_t size);
    ctx_err end(ctx_id_t env, const Attribute& attr);
    ctx_err set(ctx_id_t env, const Attribute& attr, const void* data, std::size_t size);


    // --- Attribute API

    Attribute get_attribute(ctx_id_t id) const;
    Attribute get_attribute(const std::string& name) const;

    Attribute create_attribute(const std::string& name, ctx_attr_type type, int prop = CTX_ATTR_DEFAULT);


    // --- Query API

    std::vector<RecordMap> unpack(const uint64_t buf[], std::size_t size) const;


    // --- Serialization / data access API

    void foreach_node(std::function<void(const Node&)>);
    void foreach_attribute(std::function<void(const Attribute&)>);


    // --- Caliper singleton API

    static Caliper* instance();
    static Caliper* try_instance();
};

} // namespace cali

#endif // CALI_CALIPER_H

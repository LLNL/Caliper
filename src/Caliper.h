/** 
 * @file caliper.h 
 * Initialization function and global data declaration
 */

#ifndef CALI_CALIPER_H
#define CALI_CALIPER_H

#include "cali_types.h"

#include "Attribute.h"
#include "Query.h"

#include <memory>
#include <utility>
#include <vector>

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

    ctx_err begin(ctx_id_t env, const Attribute& attr, const void* data, std::size_t size);
    ctx_err end(ctx_id_t env, const Attribute& attr);
    ctx_err set(ctx_id_t env, const Attribute& attr, const void* data, std::size_t size);


    // --- Attribute API

    std::pair<bool, Attribute> get_attribute(ctx_id_t id) const;
    std::pair<bool, Attribute> get_attribute(const std::string& name) const;

    Attribute create_attribute(const std::string& name, ctx_attr_type type, int prop = CTX_ATTR_DEFAULT);


    // --- Query API

    class QueryKey 
    {
        ctx_id_t m_attr;
        uint64_t m_value;

        QueryKey(ctx_id_t attr, uint64_t value)
            : m_attr { attr }, m_value { value }
            { }

    public:

        static QueryKey invalid;

        QueryKey() 
            : QueryKey { invalid } { }

        friend class Caliper;
    };

    std::vector<QueryKey>  unpack(const uint64_t buf[], std::size_t size) const;
    std::unique_ptr<Query> query(const QueryKey& key) const;


    // --- Caliper singleton API

    static Caliper* instance();
    static Caliper* try_instance();
};

} // namespace cali

#endif // CALI_CALIPER_H

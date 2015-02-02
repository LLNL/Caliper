/** 
 * @file caliper.h 
 * Initialization function and global data declaration
 */

#ifndef CALI_CALIPER_H
#define CALI_CALIPER_H

#include "cali_definitions.h"

#include <Attribute.h>
#include <RecordMap.h>

#include <util/callback.hpp>

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


    // --- Events

    struct Events {
        util::callback<void(Caliper*, const Attribute&)> createAttrEvt;

        util::callback<void(Caliper*, cali_id_t, const Attribute&)> beginEvt;
        util::callback<void(Caliper*, cali_id_t, const Attribute&)> endEvt;
        util::callback<void(Caliper*, cali_id_t, const Attribute&)> setEvt;

        util::callback<void(Caliper*, int)>              queryEvt;
        util::callback<void(Caliper*, int)>              tryQueryEvt;

        util::callback<void(Caliper*)>                   finishEvt;
    };

    Events&   events();

    // --- Context environment API

    cali_id_t default_environment(cali_context_scope_t context) const;
    cali_id_t current_environment(cali_context_scope_t context);

    cali_id_t create_environment();
    cali_id_t clone_environment(cali_id_t env);
    void      release_environment(cali_id_t env);

    void      set_environment_callback(cali_context_scope_t context, std::function<cali_id_t()> cb);


    // --- Context API

    std::size_t context_size(int ctx) const;
    std::size_t get_context(int ctx, uint64_t buf[], std::size_t len);


    // --- Annotation API

    cali_err  begin(const Attribute& attr, const void* data, std::size_t size);
    cali_err  end(const Attribute& attr);
    cali_err  set(const Attribute& attr, const void* data, std::size_t size);


    // --- Attribute API

    size_t    num_attributes() const;

    Attribute get_attribute(cali_id_t id) const;
    Attribute get_attribute(const std::string& name) const;

    Attribute create_attribute(const std::string& name, cali_attr_type type, int prop = CALI_ATTR_DEFAULT);


    // --- Query API

    std::vector<RecordMap> unpack(const uint64_t buf[], std::size_t size) const;


    // --- Serialization / data access API

    void      foreach_node(std::function<void(const Node&)>);
    void      foreach_attribute(std::function<void(const Attribute&)>);

    bool      write_metadata();


    // --- Caliper singleton API

    static Caliper* instance();
    static Caliper* try_instance();
};

} // namespace cali

#endif // CALI_CALIPER_H

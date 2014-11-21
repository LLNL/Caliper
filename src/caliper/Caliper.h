/** 
 * @file caliper.h 
 * Initialization function and global data declaration
 */

#ifndef CALI_CALIPER_H
#define CALI_CALIPER_H

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

        util::callback<void(Caliper*, cali_id_t)> cloneEvt;

        util::callback<void(Caliper*, cali_id_t)> queryEvt;
        util::callback<void(Caliper*, cali_id_t)> tryQueryEvt;

        util::callback<void(Caliper*)> finishEvt;
    };

    Events&   events();
    

    // --- Context API

    cali_id_t current_environment() const;
    cali_id_t clone_environment(cali_id_t env);

    void set_environment_callback(std::function<cali_id_t()> cb);

    std::size_t context_size(cali_id_t env) const;
    std::size_t get_context(cali_id_t env, uint64_t buf[], std::size_t len);


    // --- Annotation API

    cali_err begin(cali_id_t env, const Attribute& attr, const void* data, std::size_t size);
    cali_err end(cali_id_t env, const Attribute& attr);
    cali_err set(cali_id_t env, const Attribute& attr, const void* data, std::size_t size);


    // --- Attribute API

    size_t    num_attributes() const;

    Attribute get_attribute(cali_id_t id) const;
    Attribute get_attribute(const std::string& name) const;

    Attribute create_attribute(const std::string& name, cali_attr_type type, int prop = CALI_ATTR_DEFAULT);


    // --- Query API

    std::vector<RecordMap> unpack(const uint64_t buf[], std::size_t size) const;


    // --- Serialization / data access API

    void foreach_node(std::function<void(const Node&)>);
    void foreach_attribute(std::function<void(const Attribute&)>);

    bool write_metadata();


    // --- Caliper singleton API

    static Caliper* instance();
    static Caliper* try_instance();
};

} // namespace cali

#endif // CALI_CALIPER_H

/** 
 * @file caliper.h 
 * Initialization function and global data declaration
 */

#ifndef CALI_CALIPER_H
#define CALI_CALIPER_H

#include <memory>


namespace cali
{

// Forward declarations

class Caliper 
{
    struct CaliperImpl;

    std::unique_ptr<CaliperImpl> mP;


    Caliper();

public:

    void addNewAttribute(const Attribute& attr);

    static Caliper* instance();

    static Caliper* try_instance();

    static ctx_id_t get_new_node_id() { 
        return get_new_id() * 2 + 1;
    }

    static ctx_id_t get_new_attr_id() {
        return get_new_id() * 2;
    }

    static bool is_node_id(ctx_id_t id) {
        return id % 2 == 1;
    }

    static bool is_attr_id(ctx_id_t id) {
        return id % 2 == 0;
    }
};

} // namespace cali

#endif // CALI_CALIPER_H

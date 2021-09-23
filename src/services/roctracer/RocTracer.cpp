// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"

#include <roctracer.h>
#include <roctracer_ext.h>
#include <roctracer_hip.h>

using namespace cali;

namespace
{

class RocTracerService {
    Attribute m_api_attr;

    void create_callback_attributes(Caliper* c) {
        Attribute subs_attr = c->get_attribute("subscription_event");
        Variant v_true(true);

        m_api_attr =
            c->create_attribute("rocm.api", CALI_TYPE_STRING,
                                CALI_ATTR_NESTED,
                                1, &subs_attr, &v_true);
    }

    void subscribe_attributes(Caliper* c, Channel* channel) {
        channel->events().subscribe_attribute(c, channel, m_api_attr);
    }
#if 0
    static void kfd_api_callback(uint32_t domain, uint32_t cid, const void* callback_data, void* arg) {
        auto instance = static_cast<const RocTracerService*>(arg);

        Caliper c;
        auto data = static_cast<const kdf_api_data_t*>(callback_data);
        if (data->phase == ACTIVITY_API_PHASE_ENTER) {
            c.begin(instance->m_api_attr, Variant(roctracer_op_string(ACTIVITY_DOMAIN_KFD_API, cid, 0)));
        } else {
            c.end(instance->m_api_attr);
        }
    }
#endif
    static void hip_api_callback(uint32_t domain, uint32_t cid, const void* callback_data, void* arg) {
        auto instance = static_cast<const RocTracerService*>(arg);

        Caliper c;
        auto data = static_cast<const hip_api_data_t*>(callback_data);
        if (data->phase == ACTIVITY_API_PHASE_ENTER) {
            c.begin(instance->m_api_attr, Variant(roctracer_op_string(ACTIVITY_DOMAIN_HIP_API, cid, 0)));
        } else {
            c.end(instance->m_api_attr);
        }
    }

    bool init_callbacks(Channel* channel) {
        int err = roctracer_enable_domain_callback(ACTIVITY_DOMAIN_HIP_API, RocTracerService::hip_api_callback, this);
        if (err != 0)
            Log(0).stream() << channel->name() << ": roctracer: enable callback (HIP): " << roctracer_error_string() << std::endl;

        if (err == 0)
            Log(1).stream() << channel->name() << ": roctracer: enabled callbacks" << std::endl;

        return err == 0;
    }

    void stop_callbacks(Channel* channel) {
        roctracer_disable_domain_callback(ACTIVITY_DOMAIN_HIP_API);

        Log(1).stream() << channel->name() << ": roctracer: callbacks disabled" << std::endl;
    }

    void finish_cb(Caliper* c, Channel* channel) {
        stop_callbacks(channel);
    }

    RocTracerService(Caliper* c)
        : m_api_attr { Attribute::invalid }
    {
        create_callback_attributes(c);
    }


public:

    static void register_roctracer(Caliper* c, Channel* channel) {
        RocTracerService* instance = new RocTracerService(c);

        channel->events().post_init_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->subscribe_attributes(c, channel);
                instance->init_callbacks(channel);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish_cb(c, channel);
                delete instance;
            });
    }
};

} // namespace [anonymous]

namespace cali
{

CaliperService roctracer_service { "roctracer", ::RocTracerService::register_roctracer };

}
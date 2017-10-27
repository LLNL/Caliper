/// \file  Sos.cpp
/// \brief Caliper Sos Integration Service

#include "../CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/SnapshotTextFormatter.h"

#include "caliper/common/util/split.hpp"

#include <atomic>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <vector>

#include<sos.h>

using namespace cali;
using namespace std;

namespace
{

const ConfigSet::Entry   configdata[] = {
    ConfigSet::Terminator
};

void pack_snapshot(SOS_pub* sos_pub, int frame_id, const std::map< Attribute, std::vector<Variant> >& unpacked_snapshot) {
    for (auto &p : unpacked_snapshot) {
        switch (p.first.type()) {
        case CALI_TYPE_STRING:
        {
            std::string pubstr;

            for (const Variant &val : p.second)
                pubstr.append(val.to_string()).append(pubstr.empty() ? "" : "/");

            SOS_pack_frame(sos_pub, frame_id, p.first.name_c_str(), SOS_VAL_TYPE_STRING, pubstr.c_str());
        }
        break;
        case CALI_TYPE_ADDR:
        case CALI_TYPE_INT:
        case CALI_TYPE_UINT:
        case CALI_TYPE_BOOL:
        {
            int64_t val = p.second.front().to_int();
            SOS_pack_frame(sos_pub, frame_id, p.first.name_c_str(), SOS_VAL_TYPE_INT, &val);
        }
        break;
        case CALI_TYPE_DOUBLE:
        {
            double val = p.second.front().to_double();
            SOS_pack_frame(sos_pub, frame_id, p.first.name_c_str(), SOS_VAL_TYPE_DOUBLE, &val);   
        }
        default:
            ;
        }
    }

    SOS_publish(sos_pub);
}

class SosService
{
    ConfigSet                   config;
    static unique_ptr<SosService> 
                                s_sos;

    SOS_runtime *sos_runtime;
    SOS_pub *sos_publication_handle;

    // void flush_and_publish(Caliper* c) {
    //     c->flush(nullptr, [c](const SnapshotRecord* snapshot){
    //             static std::atomic<int> frame { 0 };
    //             pack_snapshot(++frame, snapshot->unpack(*c));
    //         });
    // }

    void process_snapshot(Caliper* c, const SnapshotRecord* trigger_info, const SnapshotRecord* snapshot) {
        static std::atomic<int> frame { 0 };

        pack_snapshot(sos_publication_handle, ++frame, snapshot->unpack(*c));
    }

    // Initialize the SOS runtime, and create our publication handle
    void post_init(Caliper* c) {
      sos_runtime = NULL;
      SOS_init(NULL, NULL, &sos_runtime, SOS_ROLE_CLIENT, SOS_RECEIVES_NO_FEEDBACK, NULL);
      SOS_pub_create(sos_runtime, &sos_publication_handle, (char *)"caliper.data", SOS_NATURE_CREATE_OUTPUT);
    }

    // static callbacks

    static void process_snapshot_cb(Caliper* c, const SnapshotRecord* trigger_info, const SnapshotRecord* snapshot) {
        s_sos->process_snapshot(c, trigger_info, snapshot);
    }

    static void post_init_cb(Caliper* c) { 
        s_sos->post_init(c);
    }

    SosService(Caliper* c)
        : config(RuntimeConfig::init("sos", configdata))
        {
            c->events().post_init_evt.connect(&SosService::post_init_cb);
            c->events().process_snapshot.connect(&SosService::process_snapshot_cb);

            Log(1).stream() << "Registered SOS service" << std::endl;
        }

public:

    static void sos_register(Caliper* c) {
        s_sos.reset(new SosService(c));
    }

}; // SosService

unique_ptr<SosService> SosService::s_sos { nullptr };

} // namespace

namespace cali
{
    CaliperService sos_service = { "sos", ::SosService::sos_register };
} // namespace cali

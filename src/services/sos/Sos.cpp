/// \file  Sos.cpp
/// \brief Caliper Sos Integration Service

#include "../CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/SnapshotTextFormatter.h"

#include "caliper/common/util/split.hpp"

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

class SosService
{
    ConfigSet                   config;
    static unique_ptr<SosService> 
                                s_sos;

    SOS_runtime *sos_runtime;
    SOS_pub *sos_publication_handle;

    // what is the associated SOS type enumeration for a given
    // Caliper attribute?
    typedef std::map<cali_id_t, SOS_val_type> AttrIdPackTypeMap;
    AttrIdPackTypeMap attr_to_sos_type;

    // Given an Attribute, look up
    // 1) Whether it is something we should forward to SOS
    // 2) Whether if so, what type to forward it as
    //
    // Second part isn't currently used
    static std::pair<bool,SOS_val_type> sos_type_for_cali_type(cali::Attribute attr){
      switch( attr.type() ){
        case CALI_TYPE_INV: return std::make_pair(false,SOS_VAL_TYPE_BYTES); 
        case CALI_TYPE_USR: return std::make_pair(false,SOS_VAL_TYPE_BYTES); 
        case CALI_TYPE_TYPE: return std::make_pair(false,SOS_VAL_TYPE_BYTES); 
        case CALI_TYPE_INT: return std::make_pair(true,SOS_VAL_TYPE_INT); 
        case CALI_TYPE_UINT: return std::make_pair(true,SOS_VAL_TYPE_INT); 
        case CALI_TYPE_STRING: return std::make_pair(true,SOS_VAL_TYPE_STRING); 
        case CALI_TYPE_ADDR: return std::make_pair(true,SOS_VAL_TYPE_INT); 
        case CALI_TYPE_DOUBLE: return std::make_pair(true,SOS_VAL_TYPE_INT); 
        case CALI_TYPE_BOOL: return std::make_pair(true,SOS_VAL_TYPE_INT); 
      }
    }

    void create_attribute(Caliper* c, const Attribute& attr) {
        //if (attr.skip_events())
        //    return;
        std::pair<bool,SOS_val_type> validAndType = sos_type_for_cali_type(attr);
        // If this is something we should forward
        if(validAndType.first){
          // Say what we should forward it as
          attr_to_sos_type.insert(std::make_pair(attr.id(), validAndType.second));
        }
    }

    void process_snapshot(Caliper* c, const SnapshotRecord* trigger_info, const SnapshotRecord* snapshot) {

        auto unpacked = snapshot->unpack(*c);

        // For each Attribute we're unpacking, there are a list of
        // Variants representing the nested values set on that
        // Attribute
        for(std::pair<const Attribute,std::vector<Variant>> & iter : unpacked){
          auto search = attr_to_sos_type.find(iter.first.id());
          // If this is an Attribute we know to forward
          if(search != attr_to_sos_type.end()){
            //Iterate over all of its nested values
            for(auto item : iter.second){
              // Get them as a C string for SOS
              std::string inner_string = item.to_string();
              const char* stringData = inner_string.c_str();
              //std::cout << stringData << "\n";
              // Pack it in
              SOS_pack(sos_publication_handle,iter.first.name().c_str(),SOS_VAL_TYPE_STRING,(void*)stringData);
            }
          }
        }

        // And publish
        SOS_publish(sos_publication_handle);
    }

    // Initialize the SOS runtime, and create our publication handle
    void post_init(Caliper* c) {
      sos_runtime = NULL;
      SOS_init(NULL, NULL, &sos_runtime, SOS_ROLE_CLIENT, SOS_RECEIVES_NO_FEEDBACK, NULL);
      SOS_pub_create(sos_runtime, &sos_publication_handle, (char *)"caliper.data", SOS_NATURE_CREATE_OUTPUT);
    }

    // static callbacks

    static void create_attr_cb(Caliper* c, const Attribute& attr) {
        s_sos->create_attribute(c, attr);
    }

    static void process_snapshot_cb(Caliper* c, const SnapshotRecord* trigger_info, const SnapshotRecord* snapshot) {
        s_sos->process_snapshot(c, trigger_info, snapshot);
    }

    static void post_init_cb(Caliper* c) { 
        s_sos->post_init(c);
    }

    SosService(Caliper* c)
        : config(RuntimeConfig::init("sos", configdata))
        { 

            c->events().create_attr_evt.connect(&SosService::create_attr_cb);
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

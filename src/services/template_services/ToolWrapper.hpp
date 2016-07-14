#ifndef CALI_TOOL_WRAPPER_HPP
#define CALI_TOOL_WRAPPER_HPP
#include "../CaliperService.h"
#include <caliper-config.h>
#include <Caliper.h>
#include <iostream>
#include <Log.h>
#include <RuntimeConfig.h>
namespace cali{

struct DefaultFilter{
    static bool filterAttribute(const Attribute& attr){
        return true;
    }
};

template<class ProfilerType,class FilterType=DefaultFilter>
class ToolWrapper {
    public:
    static void beginCallback(Caliper* c, const Attribute& attr, const Variant& value){
        if(FilterType::filterAttribute(attr)){
                ProfilerType::beginAction(c,attr,value);
        }
    }
    static void endCallback(Caliper* c, const Attribute& attr, const Variant& value){
        if(FilterType::filterAttribute(attr)){
                ProfilerType::endAction(c,attr,value);
        }
    }
    static void setCallbacks(Caliper* c){
        //TODO: process config
        ProfilerType::initialize();
        c->events().pre_begin_evt.connect(&beginCallback);
        c->events().pre_end_evt.connect(&endCallback);
        Log(1).stream() << "Registered "<<ProfilerType::service_name()<<" service "<<std::endl;
    }
};

#ifdef CALIPER_HAVE_NVVP
#include "nvToolsExt.h"

        const uint32_t colors[] = { 0x0000ff00, 0x000000ff, 0x00ffff00, 0x00ff00ff, 0x0000ffff, 0x00ff0000, 0x00ffffff };
        const int num_colors = sizeof(colors)/sizeof(uint32_t);

#define CALIPER_NVVP_PUSH_RANGE(name,cid) { \
            int color_id = cid; \
            color_id = color_id%num_colors;\
            nvtxEventAttributes_t eventAttrib = {0}; \
            eventAttrib.version = NVTX_VERSION; \
            eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE; \
            eventAttrib.colorType = NVTX_COLOR_ARGB; \
            eventAttrib.color = colors[color_id]; \
            eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII; \
            eventAttrib.message.ascii = name; \
            nvtxRangePushEx(&eventAttrib); \
}
#define CALIPER_NVVP_POP_RANGE nvtxRangePop();

class NVVPWrapper : public ToolWrapper<NVVPWrapper> {
    public:
    void initialize(){}
    static std::string service_name() { 
        return "NVVP service";
    }
    static void beginAction(Caliper* c, const Attribute &attr, const Variant& value){
        std::cout<<attr.name()<<","<<value.to_string()<<std::endl;
        CALIPER_NVVP_PUSH_RANGE(value.to_string().c_str(),1)
    }
    static void endAction(Caliper* c, const Attribute& attr, const Variant& value){
         CALIPER_NVVP_POP_RANGE
    }
};

CaliperService NVVPTriggerService { "nvvp", &NVVPWrapper::setCallbacks};

}
#endif

#ifdef CALIPER_HAVE_VTUNE
#include <ittnotify.h>

class ITTWrapper : public ToolWrapper<ITTWrapper> {
    public:
    static itt_domain* domain;
    static std::string service_name(){
        return "VTune Service";
    }
    static void beginAction(Caliper* c, const Attribute &attr, const Variant& value){
       __itt_task_begin(domain, __itt_null, __itt_null, __itt_string_handle_create(value.to_string()));
    }
    static void endAction(Caliper* c, const Attribute& attr, const Variant& value){
       __itt_task_end(domain);
    }
    void initalize(){
        domain = __itt_domain_create("Caliper Instrumentation Domain");
    }
}

CaliperService ITTTriggerService { "vtune", &ITTWrapper::setCallbacks }

#endif
#endif

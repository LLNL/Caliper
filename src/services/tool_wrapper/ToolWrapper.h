#ifndef CALI_TOOL_WRAPPER_HPP
#define CALI_TOOL_WRAPPER_HPP
#include "../CaliperService.h"
#include <caliper-config.h>
#include <Caliper.h>
#include <iostream>
#include <Log.h>
#include <RuntimeConfig.h>

#include "../filters/DefaultFilter.h"

namespace cali {

template<class ProfilerType,class FilterType=DefaultFilter>
class ToolWrapper {
    public:
    static void beginCallback(Caliper* c, const Attribute& attr, const Variant& value){
        if(FilterType::filter(attr, value)){
                ProfilerType::beginAction(c,attr,value);
        }
    }
    static void endCallback(Caliper* c, const Attribute& attr, const Variant& value){
        if(FilterType::filter(attr, value)){
                ProfilerType::endAction(c,attr,value);
        }
    }
    static void setCallbacks(Caliper* c){
        ProfilerType::initialize();
        c->events().pre_begin_evt.connect(&beginCallback);
        c->events().pre_end_evt.connect(&endCallback);
        Log(1).stream() << "Registered "<<ProfilerType::service_name()<<" service "<<std::endl;
    }
};

}

#endif

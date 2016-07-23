#ifndef CALI_TOOL_WRAPPER_HPP
#define CALI_TOOL_WRAPPER_HPP
#include "../CaliperService.h"
#include <caliper-config.h>
#include <Caliper.h>
#include <iostream>
#include <Log.h>
#include <RuntimeConfig.h>

#include "common/filters/DefaultFilter.h"

namespace cali {

template<class ProfilerType,class FilterType=DefaultFilter>
class ToolWrapper {
    public:
    virtual void beginCallback(Caliper* c, const Attribute& attr, const Variant& value){
        if(FilterType::filter(attr, value)){
                this->beginAction(c,attr,value);
        }
    }
    virtual void endCallback(Caliper* c, const Attribute& attr, const Variant& value){
        if(FilterType::filter(attr, value)){
                this->endAction(c,attr,value);
        }
    }
    virtual void beginAction(Caliper* c, const Attribute& attr, const Variant& value) {}
    
    virtual void endAction(Caliper* c, const Attribute& attr, const Variant& value) {}
};

template <class ProfilerType,class FilterType=DefaultFilter>
static void setCallbacks(Caliper* c){
    FilterType::initialize();
    ProfilerType* newProfiler = new ProfilerType();
    newProfiler->initialize();
    //c->events().pre_begin_evt.connect(&(newProfiler->beginAction));
    c->events().pre_begin_evt.connect([=](Caliper* c,const Attribute& attr,const Variant& value){
        newProfiler->beginCallback(c,attr,value);
    });
    c->events().pre_end_evt.connect([=](Caliper* c,const Attribute& attr,const Variant& value){
        newProfiler->endCallback(c,attr,value);
    });
    c->events().finish_evt.connect([=](Caliper* c){
        delete newProfiler;
    });
    //c->events().pre_end_evt.connect(&endCallback);
    Log(1).stream() << "Registered "<<newProfiler->service_name()<<" service "<<std::endl;
}

}

#endif

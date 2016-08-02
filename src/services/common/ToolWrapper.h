#ifndef CALI_TOOL_WRAPPER_HPP
#define CALI_TOOL_WRAPPER_HPP
#include "../CaliperService.h"
#include <caliper-config.h>
#include <Caliper.h>
#include <iostream>
#include <Log.h>
#include <RuntimeConfig.h>

#include "common/filters/Filter.h"
#include "common/filters/DefaultFilter.h"
#include "common/filters/RegexFilter.h"

namespace cali {

static cali::ConfigSet::Entry s_configdata[] = {
  { "regex", CALI_TYPE_STRING, "",
    "Regular expression for matching annotations",
    "Regular expression for matching annotations"
  },
  {
    "inclusive", CALI_TYPE_BOOL, "true",
    "Whether the regular expression should include or exclude annotations",
    "Whether the regular expression should include or exclude annotations"
  },
  cali::ConfigSet::Terminator
};

template<class ProfilerType>
class ToolWrapper {
    protected:
    cali::ConfigSet config;
    Filter<ProfilerType>* filter;
    public:
    virtual std::string service_tag(){
        throw std::runtime_error("Caliper services must implement service_tag");
    }
    virtual void beginCallback(Caliper* c, const Attribute& attr, const Variant& value){
        if(filter->filter(attr, value)){
                this->beginAction(c,attr,value);
        }
    }
    Filter<ProfilerType>* getFilter(){
        if(config.get("regex").to_string()==""){
           filter = new DefaultFilter();
        }
        filter = new RegexFilter();
        filter->configure();
        return filter;
    }
    //virtual void do_initiailize(){
    //    std::string name = service_tag();
    //    initialize(name);
    //}
    virtual void initialize(std::string name) {}
    virtual void endCallback(Caliper* c, const Attribute& attr, const Variant& value){
        if(filter->filter(attr, value)){
                this->endAction(c,attr,value);
        }
    }
    virtual void beginAction(Caliper* c, const Attribute& attr, const Variant& value) {}
    
    virtual void endAction(Caliper* c, const Attribute& attr, const Variant& value) {}
};

template <class ProfilerType>
static void setCallbacks(Caliper* c){
    ProfilerType* newProfiler = new ProfilerType();
    //newProfiler->do_initialize();
    newProfiler->initialize();

    Filter<ProfilerType>* newFilter = newProfiler->getFilter();
    newFilter->do_initialize(newProfiler->service_tag());
    c->events().pre_begin_evt.connect([=](Caliper* c,const Attribute& attr,const Variant& value){
        newProfiler->beginCallback(c,attr,value);
    });
    c->events().pre_end_evt.connect([=](Caliper* c,const Attribute& attr,const Variant& value){
        newProfiler->endCallback(c,attr,value);
    });
    c->events().finish_evt.connect([=](Caliper* c){
        delete newProfiler;
        delete newFilter;
    });
    //c->events().pre_end_evt.connect(&endCallback);
    Log(1).stream() << "Registered "<<newProfiler->service_name()<<" service "<<std::endl;
}

}

#endif

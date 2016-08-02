#ifndef CALI_SERVICES_FILTER_HXX_
#define CALI_SERVICES_FILTER_HXX_

#include "Caliper.h"
class Filter {
  protected:
    cali::ConfigSet config;
  public:
    virtual bool filter(const cali::Attribute& attr, const cali::Variant& value ) 
    {
      return apply_filter(attr, value);
    }
    void configure(cali::ConfigSet newConfig){
      config = newConfig;
    }
    virtual void do_initialize(std::string name){
        initialize(name);
    }
    virtual bool apply_filter(const cali::Attribute& attr, const cali::Variant& value) = 0;
    virtual void initialize(std::string name) = 0;
};

#endif

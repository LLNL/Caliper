#ifndef CALI_SERVICES_FILTER_DEFAULT_FILTER_HXX_
#define CALI_SERVICES_FILTER_DEFAULT_FILTER_HXX_

#include "Filter.h"

class DefaultFilter : public Filter {
  public:
    virtual void initialize(std::string name){};
    virtual bool apply_filter(const cali::Attribute& attr, const cali::Variant& value)
    {
        return true;
    }
};

#endif

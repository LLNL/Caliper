#ifndef CALI_SERVICES_FILTER_HXX_
#define CALI_SERVICES_FILTER_HXX_

#include "Caliper.h"

template<class FilterType>
class Filter {
  public:
    static bool filter(const cali::Attribute& attr, const cali::Variant& value ) 
    {
      return FilterType::apply_filter(attr, value);
    }
};

#endif

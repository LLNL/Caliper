#ifndef CALI_SERVICES_FILTER_HXX_
#define CALI_SERVICES_FILTER_HXX_

#include "Attribute.h"
#include "Value.h"

template<class FilterType>
class Filter {
  public:
    static bool filter(const Attribute& attr, const Variant& value ) 
    {
      return FilterType::apply_filter(attr, value);
    }
};

#endif

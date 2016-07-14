#ifndef CALI_SERVICES_FILTER_REGEX_FILTER_HXX_
#define CALI_SERVICES_FILTER_REGEX_FILTER_HXX_

#include "Filter.h"

#include <string>
#include <sstream>
#include <regex>

class DefaultFilter : Filter<RegexFilter> {
  public:
    static bool apply_filter(const Attribute& attr, const Variant& value)
    {
        return true;
    }
};

#endif

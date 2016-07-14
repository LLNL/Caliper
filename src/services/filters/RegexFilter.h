#ifndef CALI_SERVICES_FILTER_REGEX_FILTER_HXX_
#define CALI_SERVICES_FILTER_REGEX_FILTER_HXX_

#include "Filter.h"

#include <string>
#include <sstream>
#include <regex>

class RegexFilter : Filter<RegexFilter> {
  public:
    static bool apply_filter(const Attribute& attr, const Variant& value)
    {
      std::stringstream ss;
      ss << attr.name() << "=" << value.to_string();

      std::string attr_and_val = ss.str();

      if (std::regex_search(attr_and_val, filter_regex)) {
        return true;
      } else {
        return false;
      }
    }
};

#endif

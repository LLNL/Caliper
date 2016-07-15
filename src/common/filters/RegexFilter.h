#ifndef CALI_SERVICES_FILTER_REGEX_FILTER_HXX_
#define CALI_SERVICES_FILTER_REGEX_FILTER_HXX_

#include "Filter.h"

#include <string>
#include <sstream>
#include <regex>

class RegexFilter : public Filter<RegexFilter> {
  public:
    static bool apply_filter(const cali::Attribute& attr, const cali::Variant& value)
    {
      std::stringstream ss;
      ss << attr.name() << "=" << value.to_string();

      std::string attr_and_val = ss.str();

      std::regex filter_regex("function=.*",  std::regex_constants::basic);

      if (std::regex_search(attr_and_val, filter_regex)) {
        return true;
      } else {
        return false;
      }
    }
};

#endif

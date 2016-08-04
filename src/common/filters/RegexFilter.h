#ifndef CALI_SERVICES_FILTER_REGEX_FILTER_HXX_
#define CALI_SERVICES_FILTER_REGEX_FILTER_HXX_

#include "Filter.h"

#include <string>
#include <sstream>
#include <regex>

#include <iostream>


class RegexFilter : public Filter {
  private:
    std::string regex;
    bool inclusive;
  public:
    virtual void initialize(std::string config_name)
    {
      regex = config.get("regex").to_string();
      inclusive = config.get("inclusive").to_bool();
    }

    virtual bool apply_filter(const cali::Attribute& attr, const cali::Variant& value)
    {
      std::stringstream ss;
      ss << attr.name() << "=" << value.to_string();

      std::string attr_and_val = ss.str();

      std::regex filter_regex(regex,  std::regex_constants::extended);

      if (std::regex_search(attr_and_val, filter_regex)) {
        return inclusive;
      } else {
        return (!inclusive);
      }
    }
};

#endif

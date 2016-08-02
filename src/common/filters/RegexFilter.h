#ifndef CALI_SERVICES_FILTER_REGEX_FILTER_HXX_
#define CALI_SERVICES_FILTER_REGEX_FILTER_HXX_

#include "Filter.h"

#include <string>
#include <sstream>
#include <regex>

#include <iostream>

namespace {

    static const cali::ConfigSet::Entry s_configdata[] = {
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

}

class RegexFilter : public Filter<RegexFilter> {
  private:
    static std::string regex;
    static cali::ConfigSet config;
    static bool inclusive;
  public:
    static void initialize()
    {
      config = cali::RuntimeConfig::init("tau", s_configdata);
      regex = config.get("regex").to_string();
      inclusive = config.get("inclusive").to_bool();
      std::cout<<"REGEX: "<<regex<<std::endl;
    }

    static bool apply_filter(const cali::Attribute& attr, const cali::Variant& value)
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

bool RegexFilter::inclusive;
std::string RegexFilter::regex;
cali::ConfigSet RegexFilter::config;

#endif

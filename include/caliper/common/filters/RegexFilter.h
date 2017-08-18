#pragma once

#include "Filter.h"

#include <string>
#include <regex>

#include <iostream>

namespace cali
{

class RegexFilter : public Filter {
    std::regex m_filter_regex;
    bool       m_inclusive;

public:

    RegexFilter(const char* tag, const cali::ConfigSet& config)
        {
            std::string regex = config.get("regex").to_string();

            m_inclusive    = config.get("inclusive").to_bool();
            m_filter_regex = std::regex(regex, std::regex::optimize);            
        }

    virtual bool filter(const cali::Attribute& attr, const cali::Variant& value) {
        std::string s(attr.name());
        s.append("=");
        s.append(value.to_string());

        if (std::regex_search(s, m_filter_regex)) {
            return m_inclusive;
        } else {
            return (!m_inclusive);
        }
    }
};

}


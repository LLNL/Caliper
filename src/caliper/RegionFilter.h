// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file RegionFilter.h
/// Implement region filtering
///

#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace cali
{

class Variant;

/// \brief Implements region (string) filtering
class RegionFilter
{
    struct Filter {
        std::vector< std::string > startswith;
        std::vector< std::string > match;
        std::vector< std::regex  > regex;
    };

    std::shared_ptr<Filter> m_include_filters;
    std::shared_ptr<Filter> m_exclude_filters;

    static std::pair<std::shared_ptr<Filter>, std::string> parse_filter_config(std::istream& is);

    static bool match(const Variant& val, const Filter&);

    RegionFilter(std::shared_ptr<Filter> iflt, std::shared_ptr<Filter> eflt)
        : m_include_filters { iflt },
          m_exclude_filters { eflt }
        { }

public:

    bool pass(const Variant& val) const {
        if (m_exclude_filters)
            if (match(val, *m_exclude_filters))
                return false;
        if (m_include_filters)
            return match(val, *m_include_filters);

        return true;
    }

    RegionFilter()
        { }

    static std::pair<RegionFilter, std::string> from_config(const std::string& include, const std::string& exclude);
};

} // namespace cali

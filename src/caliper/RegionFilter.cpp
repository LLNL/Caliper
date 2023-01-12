// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "RegionFilter.h"

#include "caliper/common/Variant.h"

#include "../common/util/parse_util.h"

#include <iostream>
#include <sstream>

using namespace cali;

namespace
{

class ArgumentListParser
{
    std::string m_error_msg;
    bool m_error;

public:

    ArgumentListParser()
        : m_error_msg {}, m_error { false }
    { }

    bool error() const { return m_error; }
    std::string error_msg() const { return m_error_msg; }

    std::vector<std::string> parse(std::istream& is) {
        std::vector<std::string> ret;
        char c = util::read_char(is);

        if (c != '(') {
            is.unget();
            return ret;
        }

        do {
            std::string word = util::read_word(is, ",()");

            if (!word.empty())
                ret.emplace_back(std::move(word));

            c = util::read_char(is);
        } while (!m_error && is.good() && c == ',');

        if (c != ')') {
            m_error = true;
            m_error_msg = "missing \')\'";
        }

        return ret;
    }
};

}

std::pair<std::shared_ptr<RegionFilter::Filter>, std::string>
RegionFilter::parse_filter_config(std::istream& is)
{
    Filter ret;

    bool error = false;
    std::string error_msg;

    char c = 0;

    do {
        std::string word = util::read_word(is, ",()");

        if (word == "match") {
            ::ArgumentListParser argparse;
            auto args = argparse.parse(is);
            if (!argparse.error()) {
                ret.match.insert(ret.match.end(), args.begin(), args.end());
            } else {
                error = true;
                error_msg = std::string("in match(): ") + argparse.error_msg();
            }
        } else if (word == "startswith") {
            ::ArgumentListParser argparse;
            auto args = argparse.parse(is);
            if (!argparse.error()) {
                ret.startswith.insert(ret.startswith.end(), args.begin(), args.end());
            } else {
                error = true;
                error_msg = std::string("in startswith(): ") + argparse.error_msg();
            }
        } else if (word == "regex") {
            ::ArgumentListParser argparse;
            auto args = argparse.parse(is);
            if (!argparse.error()) {
                try {
                    for (const auto &s : args)
                        ret.regex.push_back(std::regex(s));
                } catch (const std::regex_error& e) {
                    error = true;
                    error_msg = e.what();
                }
            } else {
                error = true;
                error_msg = std::string("in regex(): ") + argparse.error_msg();
            }
        } else if (!word.empty()) {
            ret.match.push_back(word);
        }

        c = util::read_char(is);
    } while (!error && is.good() && c == ',');

    if (is.good())
        is.unget();

    std::shared_ptr<Filter> retp;
    if (!error && !(ret.match.empty() && ret.startswith.empty() && ret.regex.empty()))
        retp = std::make_shared<Filter>(std::move(ret));

    return std::make_pair(retp, error_msg);
}

bool
RegionFilter::match(const Variant& val, const Filter& filter)
{
    //   We assume val is a string. Variant strings aren't
    // 0-terminated, hence the more complicated comparisons
    const char* strp = static_cast<const char*>(val.data());

    for (const auto &w : filter.startswith)
        if (val.size() >= w.size() && w.compare(0, w.size(), strp, w.size()) == 0)
            return true;

    for (const auto &w : filter.match)
        if (val.size() == w.size() && w.compare(0, w.size(), strp, w.size()) == 0)
            return true;

    for (const auto &r : filter.regex)
        if (std::regex_match(std::string(strp, val.size()), r) == true)
            return true;

    return false;
}

std::pair<RegionFilter, std::string>
RegionFilter::from_config(const std::string& include, const std::string& exclude)
{
    std::shared_ptr<Filter> icfg;
    std::shared_ptr<Filter> ecfg;

    {
        std::istringstream is(include);
        auto p = parse_filter_config(is);

        if (!p.second.empty())
            return std::make_pair(RegionFilter(), p.second);

        icfg = p.first;
    }

    {
        std::istringstream is(exclude);
        auto p = parse_filter_config(is);

        if (!p.second.empty())
            return std::make_pair(RegionFilter(), p.second);

        ecfg = p.first;
    }

    return std::make_pair(RegionFilter(icfg, ecfg), std::string());
}

#include "TopdownCalculator.h"

#include "caliper/common/Log.h"

#include <algorithm>

namespace cali {
namespace topdown {

Variant TopdownCalculator::get_val_from_rec(const std::vector<Entry> &rec,
                                            const char *name) {
  Variant ret;

  auto c_it = m_counter_attrs.find(name);
  if (c_it == m_counter_attrs.end())
    return ret;

  cali_id_t attr_id = c_it->second.id();

  auto it = std::find_if(rec.begin(), rec.end(), [attr_id](const Entry &e) {
    return e.attribute() == attr_id;
  });

  if (it != rec.end())
    ret = it->value();
  else
    ++m_counters_not_found[std::string(name)];

  return ret;
}

TopdownCalculator::TopdownCalculator(IntelTopdownLevel level,
                                     const char *top_counters,
                                     const char *all_counters,
                                     std::vector<const char *> &&res_top,
                                     std::vector<const char *> &&res_all)
    : m_level(level), m_top_counters(top_counters),
      m_all_counters(all_counters), m_res_top(res_top), m_res_all(res_all) {}

TopdownCalculator::TopdownCalculator(IntelTopdownLevel level)
    : m_level(level) {}

bool TopdownCalculator::find_counter_attrs(CaliperMetadataAccessInterface &db) {
  const char *list = (m_level == All ? m_all_counters : m_top_counters);
  auto counters = StringConverter(list).to_stringlist();

  for (const auto &s : counters) {
    Attribute attr = db.get_attribute(std::string("sum#papi.") + s);

    if (attr == Attribute::invalid)
      attr = db.get_attribute(std::string("papi.") + s);
    if (attr == Attribute::invalid) {
      Log(0).stream() << "topdown: " << s << " counter attribute not found!"
                      << std::endl;
      return false;
    }

    m_counter_attrs[s] = attr;
  }

  return true;
}

void TopdownCalculator::make_result_attrs(CaliperMetadataAccessInterface &db) {
  std::vector<const char *> &res = (m_level == Top ? m_res_top : m_res_all);

  for (const char *s : res) {
    m_result_attrs[std::string(s)] =
        db.create_attribute(std::string("topdown.") + s, CALI_TYPE_DOUBLE,
                            CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
  }
}

const std::map<std::string, int> &
TopdownCalculator::get_counters_not_found() const {
  return m_counters_not_found;
}

const char *TopdownCalculator::get_counters() const {
  if (m_level == All) {
    return m_all_counters;
  } else {
    return m_top_counters;
  }
}

IntelTopdownLevel TopdownCalculator::get_level() const { return m_level; }

} // namespace topdown
} // namespace cali
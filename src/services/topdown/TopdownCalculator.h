#ifndef CALI_TOPDOWN_TOPDOWN_CALCULATOR_H
#define CALI_TOPDOWN_TOPDOWN_CALCULATOR_H

#include "caliper/Caliper.h"

#include <map>
#include <vector>

namespace cali {
namespace topdown {

enum IntelTopdownLevel { All = 1, Top = 2 };

class TopdownCalculator {
protected:
  IntelTopdownLevel m_level;

  const char *m_top_counters;
  const char *m_all_counters;

  std::vector<const char *> m_res_top;
  std::vector<const char *> m_res_all;

  std::map<std::string, Attribute> m_counter_attrs;
  std::map<std::string, Attribute> m_result_attrs;

  std::map<std::string, int> m_counters_not_found;

  Variant get_val_from_rec(const std::vector<Entry> &rec, const char *name);

  TopdownCalculator(IntelTopdownLevel level, const char *top_counters,
                    const char *all_counters,
                    std::vector<const char *> &&res_top,
                    std::vector<const char *> &&res_all);

public:
  TopdownCalculator(IntelTopdownLevel level);

  virtual ~TopdownCalculator() = default;

  virtual bool check_for_disabled_multiplex() const = 0;

  virtual std::vector<Entry>
  compute_toplevel(const std::vector<Entry> &rec) = 0;

  virtual std::size_t get_num_expected_toplevel() const = 0;

  virtual std::vector<Entry>
  compute_retiring(const std::vector<Entry> &rec) = 0;

  virtual std::size_t get_num_expected_retiring() const = 0;

  virtual std::vector<Entry>
  compute_backend_bound(const std::vector<Entry> &rec) = 0;

  virtual std::size_t get_num_expected_backend_bound() const = 0;

  virtual std::vector<Entry>
  compute_frontend_bound(const std::vector<Entry> &rec) = 0;

  virtual std::size_t get_num_expected_frontend_bound() const = 0;

  virtual std::vector<Entry>
  compute_bad_speculation(const std::vector<Entry> &rec) = 0;

  virtual std::size_t get_num_expected_bad_speculation() const = 0;

  bool find_counter_attrs(CaliperMetadataAccessInterface &db);

  void make_result_attrs(CaliperMetadataAccessInterface &db);

  const std::map<std::string, int> &get_counters_not_found() const;

  const char *get_counters() const;

  IntelTopdownLevel get_level() const;
};

} // namespace topdown
} // namespace cali

#endif /* CALI_TOPDOWN_TOPDOWN_CALCULATOR_H */
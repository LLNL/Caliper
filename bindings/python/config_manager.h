#ifndef CALI_INTERFACE_PYTHON_CONFIG_MANAGER_H
#define CALI_INTERFACE_PYTHON_CONFIG_MANAGER_H

#include "common.h"

#include <caliper/ConfigManager.h>

namespace cali {
class PythonConfigManager : public cali::ConfigManager {
public:
  PythonConfigManager();

  PythonConfigManager(const char *config_str);

  // void add_config_spec(const char *json);

  void add_config_spec(py::dict json);

  // void add_option_spec(const char *json);

  void add_option_spec(py::dict json);

  void py_add(const char *config_string);

  // void load(const char *filename);

  // void set_default_parameter(const char *key, const char *value);

  // void set_default_parameter_for_config(const char *config, const char *key,
  //                                       const char *value);

  // bool error() const;

  // Set to __repr__
  // bool error_msg() const;

  // void start();

  // void stop();

  // void flush();

  void check(const char *config_str);

  // std::vector<std::string> available_config_specs() const;

  // std::string get_documentation_for_spec(const char *name) const;

  // static std::vector<std::string> get_config_docstrings();
};

void create_caliper_config_manager_mod(py::module_ &caliper_config_manager_mod);

} // namespace cali

#endif /* CALI_INTERFACE_PYTHON_CONFIG_MANAGER_H */
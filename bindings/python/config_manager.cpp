#include "config_manager.h"
#include <stdexcept>

namespace cali {

PythonConfigManager::PythonConfigManager() : cali::ConfigManager() {}

PythonConfigManager::PythonConfigManager(const char *config_str)
    : cali::ConfigManager(config_str) {}

void PythonConfigManager::add_config_spec(py::dict json) {
  add_config_spec(py::str(json));
}

void PythonConfigManager::add_option_spec(py::dict json) {
  add_option_spec(py::str(json));
}

void PythonConfigManager::py_add(const char *config_string) {
  cali::ConfigManager::add(config_string);
}

void PythonConfigManager::check(const char *config_str) {
  std::string err_msg = cali::ConfigManager::check(config_str);
  if (err_msg.size() != 0) {
    throw std::runtime_error(err_msg);
  }
}

void create_caliper_config_manager_mod(
    py::module_ &caliper_config_manager_mod) {
  py::class_<PythonConfigManager> config_mgr_type(caliper_config_manager_mod,
                                                  "ConfigManager");
  config_mgr_type.def(py::init<>());
  config_mgr_type.def(py::init<const char *>());
  config_mgr_type.def("add_config_spec",
                      static_cast<void (PythonConfigManager::*)(const char *)>(
                          &cali::ConfigManager::add_config_spec));
  config_mgr_type.def("add_config_spec",
                      static_cast<void (PythonConfigManager::*)(py::dict)>(
                          &PythonConfigManager::add_config_spec));
  config_mgr_type.def("add_option_spec",
                      static_cast<void (PythonConfigManager::*)(const char *)>(
                          &cali::ConfigManager::add_option_spec));
  config_mgr_type.def("add_option_spec",
                      static_cast<void (PythonConfigManager::*)(py::dict)>(
                          &PythonConfigManager::add_option_spec));
  config_mgr_type.def("add", &PythonConfigManager::py_add);
  config_mgr_type.def("load",
                      static_cast<void (PythonConfigManager::*)(const char *)>(
                          &cali::ConfigManager::load));
  config_mgr_type.def(
      "set_default_parameter",
      static_cast<void (PythonConfigManager::*)(const char *, const char *)>(
          &cali::ConfigManager::set_default_parameter));
  config_mgr_type.def(
      "set_default_parameter_for_config",
      static_cast<void (PythonConfigManager::*)(const char *, const char *,
                                                const char *)>(
          &cali::ConfigManager::set_default_parameter_for_config));
  config_mgr_type.def("error",
                      static_cast<bool (PythonConfigManager::*)() const>(
                          &cali::ConfigManager::error));
  config_mgr_type.def("error_msg",
                      static_cast<std::string (PythonConfigManager::*)() const>(
                          &cali::ConfigManager::error_msg));
  config_mgr_type.def("__repr__",
                      static_cast<std::string (PythonConfigManager::*)() const>(
                          &cali::ConfigManager::error_msg));
  config_mgr_type.def("start", static_cast<void (PythonConfigManager::*)()>(
                                   &cali::ConfigManager::start));
  config_mgr_type.def("stop", static_cast<void (PythonConfigManager::*)()>(
                                  &cali::ConfigManager::stop));
  config_mgr_type.def("flush", static_cast<void (PythonConfigManager::*)()>(
                                   &cali::ConfigManager::flush));
  config_mgr_type.def("check", &PythonConfigManager::check);
  config_mgr_type.def(
      "available_config_specs",
      static_cast<std::vector<std::string> (PythonConfigManager::*)() const>(
          &cali::ConfigManager::available_config_specs));
  config_mgr_type.def(
      "get_documentation_for_spec",
      static_cast<std::string (PythonConfigManager::*)(const char *) const>(
          &cali::ConfigManager::get_documentation_for_spec));
  config_mgr_type.def_static("get_config_docstrings",
                             static_cast<std::vector<std::string> (*)()>(
                                 &cali::ConfigManager::get_config_docstrings));
}

} // namespace cali
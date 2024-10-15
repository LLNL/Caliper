#include "config_manager.h"
#include <stdexcept>

namespace cali
{

PythonConfigManager::PythonConfigManager() : cali::ConfigManager()
{}

PythonConfigManager::PythonConfigManager(const char* config_str) : cali::ConfigManager(config_str)
{}

void PythonConfigManager::add_config_spec(py::dict json)
{
    add_config_spec(py::str(json));
}

void PythonConfigManager::add_option_spec(py::dict json)
{
    add_option_spec(py::str(json));
}

void PythonConfigManager::py_add(const char* config_string)
{
    cali::ConfigManager::add(config_string);
}

void PythonConfigManager::check(const char* config_str)
{
    std::string err_msg = cali::ConfigManager::check(config_str);
    if (err_msg.size() != 0) {
        throw std::runtime_error(err_msg);
    }
}

void create_caliper_config_manager_mod(py::module_& caliper_config_manager_mod)
{
    py::class_<PythonConfigManager> config_mgr_type(caliper_config_manager_mod, "ConfigManager");
    config_mgr_type.def(py::init<>(), "Create a ConfigManager.");
    config_mgr_type.def(py::init<const char*>(), "Create a ConfigManager with the provide configuration string.");
    config_mgr_type.def(
        "add_config_spec",
        static_cast<void (PythonConfigManager::*)(const char*)>(&cali::ConfigManager::add_config_spec),
        "Add a custom config spec to this ConfigManager."
        ""
        "Adds a new Caliper configuration spec for this ConfigManager"
        "using a custom ChannelController or option checking function."
    );
    config_mgr_type.def(
        "add_config_spec",
        static_cast<void (PythonConfigManager::*)(py::dict)>(&PythonConfigManager::add_config_spec),
        "Add a JSON config spec to this ConfigManager"
        ""
        "Adds a new Caliper configuration specification for this ConfigManager"
        "using a basic ChannelController."
        ""
        "See the C++ docs for more details"
    );
    config_mgr_type.def(
        "add_option_spec",
        static_cast<void (PythonConfigManager::*)(const char*)>(&cali::ConfigManager::add_option_spec),
        "Add a JSON option spec to this ConfigManager"
        ""
        "Allows one to define options for any config in a matching category."
        "Option specifications must be added before querying or creating any"
        "configurations to be effective."
        ""
        "See the C++ docs for more details"
    );
    config_mgr_type.def(
        "add_option_spec",
        static_cast<void (PythonConfigManager::*)(py::dict)>(&PythonConfigManager::add_option_spec),
        "Add a JSON option spec to this ConfigManager"
        ""
        "Allows one to define options for any config in a matching category."
        "Option specifications must be added before querying or creating any"
        "configurations to be effective."
        ""
        "See the C++ docs for more details"
    );
    config_mgr_type.def(
        "add",
        &PythonConfigManager::py_add,
        "Parse the provided configuration string and create the "
        "specified configuration channels."
    );
    config_mgr_type.def(
        "load",
        static_cast<void (PythonConfigManager::*)(const char*)>(&cali::ConfigManager::load),
        "Load config and option specs from the provided filename."
    );
    config_mgr_type.def(
        "set_default_parameter",
        static_cast<void (PythonConfigManager::*)(const char*, const char*)>(&cali::ConfigManager::set_default_parameter
        ),
        "Pre-set a key-value pair for all configurations."
    );
    config_mgr_type.def(
        "set_default_parameter_for_config",
        static_cast<void (PythonConfigManager::*)(const char*, const char*, const char*)>(
            &cali::ConfigManager::set_default_parameter_for_config
        ),
        "Pre-set a key-value pair for the specified configuration."
    );
    config_mgr_type.def(
        "error",
        static_cast<bool (PythonConfigManager::*)() const>(&cali::ConfigManager::error),
        "Returns true if there was an error while parsing configuration."
    );
    config_mgr_type.def(
        "error_msg",
        static_cast<std::string (PythonConfigManager::*)() const>(&cali::ConfigManager::error_msg),
        "Returns an error message if there was an error while "
        "parsing configuration."
    );
    config_mgr_type.def(
        "__repr__",
        static_cast<std::string (PythonConfigManager::*)() const>(&cali::ConfigManager::error_msg)
    );
    config_mgr_type.def(
        "start",
        static_cast<void (PythonConfigManager::*)()>(&cali::ConfigManager::start),
        "Start all configured measurement channels, or re-start paused ones."
    );
    config_mgr_type.def(
        "stop",
        static_cast<void (PythonConfigManager::*)()>(&cali::ConfigManager::stop),
        "Pause all configured measurement channels."
    );
    config_mgr_type.def(
        "flush",
        static_cast<void (PythonConfigManager::*)()>(&cali::ConfigManager::flush),
        "Flush all configured measurement channels."
    );
    config_mgr_type.def("check", &PythonConfigManager::check, "Check if the given config string is valid.");
    config_mgr_type.def(
        "available_config_specs",
        static_cast<std::vector<std::string> (PythonConfigManager::*)() const>(
            &cali::ConfigManager::available_config_specs
        ),
        "Return names of available config specs."
    );
    config_mgr_type.def(
        "get_documentation_for_spec",
        static_cast<std::string (PythonConfigManager::*)(const char*) const>(
            &cali::ConfigManager::get_documentation_for_spec
        ),
        "Return short description for the given config spec."
    );
    config_mgr_type.def_static(
        "get_config_docstrings",
        static_cast<std::vector<std::string> (*)()>(&cali::ConfigManager::get_config_docstrings),
        "Return descriptions for available global configs."
    );
}

} // namespace cali
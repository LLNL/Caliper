#include "annotation.h"
#include "config_manager.h"
#include "instrumentation.h"
#include "loop.h"

bool pycaliper_is_initialized()
{
    return cali_is_initialized() != 0;
}

PYBIND11_MODULE(__pycaliper_impl, m)
{
    m.attr("__version__") = cali_caliper_version();

    m.def(
        "config_preset",
        [](std::map<const char*, const char*>& preset_map) {
            for (auto kv : preset_map) {
                cali_config_preset(kv.first, kv.second);
            }
        },
        "Pre-set a config entry in the default config."
        "The entry can still be overwritten by environment variables."
    );
    m.def(
        "init",
        &cali_init,
        "Initialize Caliper."
        "Typically, it is not necessary to initialize Caliper explicitly."
        "Caliper will lazily initialize itself on the first Caliper API call."
        "This function is used primarily by the Caliper annotation macros,"
        "to ensure that Caliper's pre-defined annotation attributes are"
        "initialized."
        "It can also be used to avoid high initialization costs in the first"
        "Caliper API call."
    );
    m.def("is_initialized", &pycaliper_is_initialized, "Check if Caliper is initialized on this process.");

    auto types_mod = m.def_submodule("types", "Special types used by lower-level Caliper APIs.");

    py::enum_<cali_attr_type> c_attr_type(types_mod, "AttrType");
    c_attr_type.value("CALI_TYPE_INV", CALI_TYPE_INV);
    c_attr_type.value("CALI_TYPE_USR", CALI_TYPE_USR);
    c_attr_type.value("CALI_TYPE_INT", CALI_TYPE_INT);
    c_attr_type.value("CALI_TYPE_UINT", CALI_TYPE_UINT);
    c_attr_type.value("CALI_TYPE_STRING", CALI_TYPE_STRING);
    c_attr_type.value("CALI_TYPE_ADDR", CALI_TYPE_ADDR);
    c_attr_type.value("CALI_TYPE_DOUBLE", CALI_TYPE_DOUBLE);
    c_attr_type.value("CALI_TYPE_BOOL", CALI_TYPE_BOOL);
    c_attr_type.value("CALI_TYPE_TYPE", CALI_TYPE_TYPE);
    c_attr_type.value("CALI_TYPE_PTR", CALI_TYPE_PTR);
    c_attr_type.export_values();

    py::enum_<cali_attr_properties> c_attr_properties(types_mod, "AttrProperties", py::arithmetic());
    c_attr_properties.value("CALI_ATTR_DEFAULT", CALI_ATTR_DEFAULT);
    c_attr_properties.value("CALI_ATTR_ASVALUE", CALI_ATTR_ASVALUE);
    c_attr_properties.value("CALI_ATTR_NOMERGE", CALI_ATTR_NOMERGE);
    c_attr_properties.value("CALI_ATTR_SCOPE_PROCESS", CALI_ATTR_SCOPE_PROCESS);
    c_attr_properties.value("CALI_ATTR_SCOPE_THREAD", CALI_ATTR_SCOPE_THREAD);
    c_attr_properties.value("CALI_ATTR_SCOPE_TASK", CALI_ATTR_SCOPE_TASK);
    c_attr_properties.value("CALI_ATTR_SKIP_EVENTS", CALI_ATTR_SKIP_EVENTS);
    c_attr_properties.value("CALI_ATTR_HIDDEN", CALI_ATTR_HIDDEN);
    c_attr_properties.value("CALI_ATTR_NESTED", CALI_ATTR_NESTED);
    c_attr_properties.value("CALI_ATTR_GLOBAL", CALI_ATTR_GLOBAL);
    c_attr_properties.value("CALI_ATTR_UNALIGNED", CALI_ATTR_UNALIGNED);
    c_attr_properties.value("CALI_ATTR_AGGREGATABLE", CALI_ATTR_AGGREGATABLE);
    c_attr_properties.value("CALI_ATTR_LEVEL_1", CALI_ATTR_LEVEL_1);
    c_attr_properties.value("CALI_ATTR_LEVEL_2", CALI_ATTR_LEVEL_2);
    c_attr_properties.value("CALI_ATTR_LEVEL_3", CALI_ATTR_LEVEL_3);
    c_attr_properties.value("CALI_ATTR_LEVEL_4", CALI_ATTR_LEVEL_4);
    c_attr_properties.value("CALI_ATTR_LEVEL_5", CALI_ATTR_LEVEL_5);
    c_attr_properties.value("CALI_ATTR_LEVEL_6", CALI_ATTR_LEVEL_6);
    c_attr_properties.value("CALI_ATTR_LEVEL_7", CALI_ATTR_LEVEL_7);
    c_attr_properties.export_values();

    auto annotation_mod = m.def_submodule("annotation", "Support for Caliper annotation APIs.");
    cali::create_caliper_annotation_mod(annotation_mod);

    auto instrumentation_mod =
        m.def_submodule("instrumentation", "Support for higher-level Caliper instrumentation APIs");
    cali::create_caliper_instrumentation_mod(instrumentation_mod);

    auto loop_mod = m.def_submodule("loop", "Support for loop annotations.");
    cali::create_caliper_loop_mod(loop_mod);

    auto config_mgr_mod = m.def_submodule("config_manager", "Support for dynamic configuration of Caliper.");
    cali::create_caliper_config_manager_mod(config_mgr_mod);
}
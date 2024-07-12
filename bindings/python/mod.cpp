#include "annotation.h"
#include "config_manager.h"
#include "instrumentation.h"
#include "loop.h"
#include "variant.h"

// TODO add cali_init, cali_is_initialized, and cali_version

bool pycaliper_is_initialized() { return cali_is_initialized() != 0; }

PYBIND11_MODULE(__pycaliper_impl, m) {
  m.attr("__version__") = cali_caliper_version();

  m.def("init", &cali_init);
  m.def("is_initialized", &pycaliper_is_initialized);

  auto types_mod = m.def_submodule("types");

  py::enum_<cali_attr_type> c_attr_type(types_mod, "AttrType");
  c_attr_type.value("INV", CALI_TYPE_INV);
  c_attr_type.value("USR", CALI_TYPE_USR);
  c_attr_type.value("INT", CALI_TYPE_INT);
  c_attr_type.value("UINT", CALI_TYPE_UINT);
  c_attr_type.value("STRING", CALI_TYPE_STRING);
  c_attr_type.value("ADDR", CALI_TYPE_ADDR);
  c_attr_type.value("DOUBLE", CALI_TYPE_DOUBLE);
  c_attr_type.value("BOOL", CALI_TYPE_BOOL);
  c_attr_type.value("TYPE", CALI_TYPE_TYPE);
  c_attr_type.value("PTR", CALI_TYPE_PTR);
  c_attr_type.export_values();

  py::enum_<cali_attr_properties> c_attr_properties(types_mod,
                                                    "AttrProperties");
  c_attr_properties.value("DEFAULT", CALI_ATTR_DEFAULT);
  c_attr_properties.value("ASVALUE", CALI_ATTR_ASVALUE);
  c_attr_properties.value("NOMERGE", CALI_ATTR_NOMERGE);
  c_attr_properties.value("SCOPE_PROCESS", CALI_ATTR_SCOPE_PROCESS);
  c_attr_properties.value("SCOPE_THREAD", CALI_ATTR_SCOPE_THREAD);
  c_attr_properties.value("SCOPE_TASK", CALI_ATTR_SCOPE_TASK);
  c_attr_properties.value("SKIP_EVENTS", CALI_ATTR_SKIP_EVENTS);
  c_attr_properties.value("HIDDEN", CALI_ATTR_HIDDEN);
  c_attr_properties.value("NESTED", CALI_ATTR_NESTED);
  c_attr_properties.value("GLOBAL", CALI_ATTR_GLOBAL);
  c_attr_properties.value("UNALIGNED", CALI_ATTR_UNALIGNED);
  c_attr_properties.value("AGGREGATABLE", CALI_ATTR_AGGREGATABLE);
  c_attr_properties.value("LEVEL_1", CALI_ATTR_LEVEL_1);
  c_attr_properties.value("LEVEL_2", CALI_ATTR_LEVEL_2);
  c_attr_properties.value("LEVEL_3", CALI_ATTR_LEVEL_3);
  c_attr_properties.value("LEVEL_4", CALI_ATTR_LEVEL_4);
  c_attr_properties.value("LEVEL_5", CALI_ATTR_LEVEL_5);
  c_attr_properties.value("LEVEL_6", CALI_ATTR_LEVEL_6);
  c_attr_properties.value("LEVEL_7", CALI_ATTR_LEVEL_7);
  c_attr_properties.export_values();

  auto variant_mod = m.def_submodule("variant");
  cali::create_caliper_variant_mod(variant_mod);

  auto annotation_mod = m.def_submodule("annotation");
  cali::create_caliper_annotation_mod(annotation_mod);

  auto instrumentation_mod = m.def_submodule("instrumentation");
  cali::create_caliper_instrumentation_mod(instrumentation_mod);

  auto loop_mod = m.def_submodule("loop");
  cali::create_caliper_loop_mod(loop_mod);

  auto config_mgr_mod = m.def_submodule("config_manager");
  cali::create_caliper_config_manager_mod(config_mgr_mod);
}
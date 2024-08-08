#include "instrumentation.h"
#include "variant.h"
#include <stdexcept>

namespace cali {

PythonAttribute::PythonAttribute(const char *name, cali_attr_type type)
    : m_attr_id(cali_create_attribute(name, type, CALI_ATTR_DEFAULT)) {
  if (m_attr_id == CALI_INV_ID) {
    throw std::runtime_error("Failed to create attribute");
  }
}

PythonAttribute::PythonAttribute(const char *name, cali_attr_type type,
                                 cali_attr_properties opt)
    : m_attr_id(cali_create_attribute(name, type, opt)) {
  if (m_attr_id == CALI_INV_ID) {
    throw std::runtime_error("Failed to create attribute");
  }
}

PythonAttribute::PythonAttribute(const char *name, cali_attr_type type,
                                 cali_attr_properties opt,
                                 std::vector<PythonAttribute &> &meta_attrs,
                                 std::vector<PythonVariant &> &meta_vals) {
  if (meta_attrs.size() != meta_vals.size()) {
    throw std::runtime_error(
        "'meta_attrs' and 'meta_vals' must be same length");
  }
  size_t num_meta_elems = meta_attrs.size();
  cali_id_t *meta_attr_list = new cali_id_t[num_meta_elems];
  cali_variant_t *meta_val_list = new cali_variant_t[num_meta_elems];
  for (size_t i = 0; i < num_meta_elems; i++) {
    meta_attr_list[i] = meta_attrs[i].m_attr_id;
    meta_val_list[i] = meta_vals[i].c_variant;
  }
  m_attr_id = cali_create_attribute_with_metadata(
      name, type, static_cast<int>(properties), num_meta_elems, meta_attr_list,
      meta_val_list);
  if (m_attr_id == CALI_INV_ID) {
    throw std::runtime_error("Could not create attribute with metadata");
  }
  delete[] meta_val_list;
  delete[] meta_attr_list;
}

PythonAttribute::PythonAttribute(cali_id_t id) {
  if (id == CALI_INV_ID) {
    throw std::runtime_error("Invalid attribute");
  }
  m_attr_id = id;
}

PythonAttribute PythonAttribute::find_attribute(const char *name) {
  PythonAttribute found_attr{cali_find_attribute(name)};
  return found_attr;
}

const char *PythonAttribute::name() const {
  return cali_attribute_name(m_attr_id);
}

cali_attr_type PythonAttribute::type() const {
  return cali_attribute_type(m_attr_id);
}

cali_attr_properties PythonAttribute::properties() const {
  return static_cast<cali_attr_properties>(
      cali_attribute_properties(m_attr_id));
}

void PythonAttribute::begin() { cali_begin(m_attr_id); }

void PythonAttribute::begin(int val) { cali_begin_int(m_attr_id, val); }

void PythonAttribute::begin(double val) { cali_begin_double(m_attr_id, val); }

void PythonAttribute::begin(const char *val) {
  cali_begin_string(m_attr_id, val);
}

void PythonAttribute::set(int val) { cali_set_int(m_attr_id, val); }

void PythonAttribute::set(double val) { cali_set_double(m_attr_id, val); }

void PythonAttribute::set(const char *val) { cali_set_string(m_attr_id, val); }

void PythonAttribute::end() { cali_end(m_attr_id); }

void create_caliper_instrumentation_mod(
    py::module_ &caliper_instrumentation_mod) {
  // PythonAttribute bindings
  py::class_<PythonAttribute> cali_attribute_type(caliper_instrumentation_mod,
                                                  "Attribute");
  cali_attribute_type.def(py::init<const char *, cali_attr_type>(), "",
                          py::arg(), py::arg());
  cali_attribute_type.def(
      py::init<const char *, cali_attr_type, cali_attr_properties>(), "",
      py::arg(), py::arg(), py::arg("opt"));
  cali_attribute_type.def(
      py::init<const char *, cali_attr_type, cali_attr_properties,
               std::vector<PythonAttribute &> &,
               std::vector<PythonVariant &> &>(),
      "");
  cali_attribute_type.def_static("find_attribute",
                                 &PythonAttribute::find_attribute);
  cali_attribute_type.def_property_readonly("name", &PythonAttribute::name);
  cali_attribute_type.def_property_readonly("type", &PythonAttribute::type);
  cali_attribute_type.def_property_readonly("properties",
                                            &PythonAttribute::properties);
  cali_attribute_type.def("begin", static_cast<void (PythonAttribute::*)()>(
                                       &PythonAttribute::begin));
  cali_attribute_type.def("begin", static_cast<void (PythonAttribute::*)(int)>(
                                       &PythonAttribute::begin));
  cali_attribute_type.def(
      "begin",
      static_cast<void (PythonAttribute::*)(double)>(&PythonAttribute::begin));
  cali_attribute_type.def("begin",
                          static_cast<void (PythonAttribute::*)(const char *)>(
                              &PythonAttribute::begin));
  cali_attribute_type.def("set", static_cast<void (PythonAttribute::*)(int)>(
                                     &PythonAttribute::set));
  cali_attribute_type.def("set", static_cast<void (PythonAttribute::*)(double)>(
                                     &PythonAttribute::set));
  cali_attribute_type.def("set",
                          static_cast<void (PythonAttribute::*)(const char *)>(
                              &PythonAttribute::set));
  cali_attribute_type.def("end", &PythonAttribute::end);

  // Bindings for region begin/end functions
  caliper_instrumentation_mod.def("begin_region", &cali_begin_region);
  caliper_instrumentation_mod.def("end_region", &cali_end_region);
  caliper_instrumentation_mod.def("begin_phase", &cali_begin_phase);
  caliper_instrumentation_mod.def("end_phase", &cali_end_phase);
  caliper_instrumentation_mod.def("begin_comm_region", &cali_begin_comm_region);
  caliper_instrumentation_mod.def("end_comm_region", &cali_end_comm_region);

  // Bindings for "_byname" functions
  caliper_instrumentation_mod.def("begin_byname", &cali_begin_byname);
  caliper_instrumentation_mod.def("begin_byname", &cali_begin_double_byname);
  caliper_instrumentation_mod.def("begin_byname", &cali_begin_int_byname);
  caliper_instrumentation_mod.def("begin_byname", &cali_begin_string_byname);
  caliper_instrumentation_mod.def("set_byname", &cali_set_double_byname);
  caliper_instrumentation_mod.def("set_byname", &cali_set_int_byname);
  caliper_instrumentation_mod.def("set_byname", &cali_set_string_byname);
  caliper_instrumentation_mod.def("end_byname", &cali_end_byname);

  // Bindings for global "_byname" functions
  caliper_instrumentation_mod.def("set_global_byname",
                                  &cali_set_global_double_byname);
  caliper_instrumentation_mod.def("set_global_byname",
                                  &cali_set_global_int_byname);
  caliper_instrumentation_mod.def("set_global_byname",
                                  &cali_set_global_string_byname);
  caliper_instrumentation_mod.def("set_global_byname",
                                  &cali_set_global_uint_byname);
}

} // namespace cali
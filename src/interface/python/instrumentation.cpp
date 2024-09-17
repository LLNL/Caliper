#include "instrumentation.h"

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
  cali_attribute_type.def(py::init<const char *, cali_attr_type>(),
                          "Create Caliper Attribute with name and type.",
                          py::arg(), py::arg());
  cali_attribute_type.def(
      py::init<const char *, cali_attr_type, cali_attr_properties>(),
      "Create Caliper Attribute with name, type, and properties.", py::arg(),
      py::arg(), py::arg("opt"));
  cali_attribute_type.def_static("find_attribute",
                                 &PythonAttribute::find_attribute,
                                 "Get Caliper Attribute by name.");
  cali_attribute_type.def_property_readonly("name", &PythonAttribute::name,
                                            "Name of the Caliper Attribute.");
  cali_attribute_type.def_property_readonly("type", &PythonAttribute::type,
                                            "Type of the Caliper Attribute.");
  cali_attribute_type.def_property_readonly(
      "properties", &PythonAttribute::properties,
      "Properties of the Caliper Attribute.");
  cali_attribute_type.def(
      "begin",
      static_cast<void (PythonAttribute::*)()>(&PythonAttribute::begin),
      "Begin region where the value for the Attribute is 'true' on the "
      "blackboard.");
  cali_attribute_type.def(
      "begin",
      static_cast<void (PythonAttribute::*)(int)>(&PythonAttribute::begin),
      "Begin integer region for attribute on the blackboard.");
  cali_attribute_type.def(
      "begin",
      static_cast<void (PythonAttribute::*)(double)>(&PythonAttribute::begin),
      "Begin float region for attribute on the blackboard.");
  cali_attribute_type.def(
      "begin",
      static_cast<void (PythonAttribute::*)(const char *)>(
          &PythonAttribute::begin),
      "Begin str/bytes region for attribute on the blackboard.");
  cali_attribute_type.def(
      "set", static_cast<void (PythonAttribute::*)(int)>(&PythonAttribute::set),
      "Set integer value for attribute on the blackboard.");
  cali_attribute_type.def(
      "set",
      static_cast<void (PythonAttribute::*)(double)>(&PythonAttribute::set),
      "Set double value for attribute on the blackboard.");
  cali_attribute_type.def(
      "set",
      static_cast<void (PythonAttribute::*)(const char *)>(
          &PythonAttribute::set),
      "Set str/bytes value for attribute on the blackboard.");
  cali_attribute_type.def(
      "end", &PythonAttribute::end,
      "End innermost open region for attribute on the blackboard.");

  // Bindings for region begin/end functions
  caliper_instrumentation_mod.def(
      "begin_region", &cali_begin_region,
      "Begin nested region by name."
      ""
      "Begins nested region using the built-in annotation attribute.");
  caliper_instrumentation_mod.def(
      "end_region", &cali_end_region,
      "End nested region by name."
      ""
      "Ends nested region using built-in annotation attribute."
      "Prints an error if the name does not match the currently open region.");
  caliper_instrumentation_mod.def(
      "begin_phase", &cali_begin_phase,
      "Begin phase region by name."
      ""
      "A phase marks high-level, long(er)-running code regions. While regular "
      "regions"
      "use the \"region\" attribute with annotation level 0, phase regions use "
      "the"
      "\"phase\" attribute with annotation level 4. Otherwise, phases behave"
      "identical to regular Caliper regions.");
  caliper_instrumentation_mod.def("end_phase", &cali_end_phase,
                                  "End phase region by name.");
  caliper_instrumentation_mod.def(
      "begin_comm_region", &cali_begin_comm_region,
      "Begin communication region by name."
      ""
      "A communication region can be used to mark communication operations"
      "(e.g., MPI calls) that belong to a single communication pattern."
      "They can be used to summarize communication pattern statistics."
      "Otherwise, they behave identical to regular Caliper regions.");
  caliper_instrumentation_mod.def("end_comm_region", &cali_end_comm_region,
                                  "End communication region by name.");

  // Bindings for "_byname" functions
  caliper_instrumentation_mod.def(
      "begin_byname", &cali_begin_byname,
      "Same as Annotation.begin, but refers to annotation by name.");
  caliper_instrumentation_mod.def(
      "begin_byname", &cali_begin_double_byname,
      "Same as Annotation.begin, but refers to annotation by name.");
  caliper_instrumentation_mod.def(
      "begin_byname", &cali_begin_int_byname,
      "Same as Annotation.begin, but refers to annotation by name.");
  caliper_instrumentation_mod.def(
      "begin_byname", &cali_begin_string_byname,
      "Same as Annotation.begin, but refers to annotation by name.");
  caliper_instrumentation_mod.def(
      "set_byname", &cali_set_double_byname,
      "Same as Annotation.set, but refers to annotation by name.");
  caliper_instrumentation_mod.def(
      "set_byname", &cali_set_int_byname,
      "Same as Annotation.set, but refers to annotation by name.");
  caliper_instrumentation_mod.def(
      "set_byname", &cali_set_string_byname,
      "Same as Annotation.set, but refers to annotation by name.");
  caliper_instrumentation_mod.def(
      "end_byname", &cali_end_byname,
      "Same as Annotation.end, but refers to annotation by name.");

  // Bindings for global "_byname" functions
  caliper_instrumentation_mod.def(
      "set_global_byname", &cali_set_global_double_byname,
      "Set a global attribute with a given name to the given value.");
  caliper_instrumentation_mod.def(
      "set_global_byname", &cali_set_global_int_byname,
      "Set a global attribute with a given name to the given value.");
  caliper_instrumentation_mod.def(
      "set_global_byname", &cali_set_global_string_byname,
      "Set a global attribute with a given name to the given value.");
  caliper_instrumentation_mod.def(
      "set_global_byname", &cali_set_global_uint_byname,
      "Set a global attribute with a given name to the given value.");
}

} // namespace cali
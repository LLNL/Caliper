#include "annotation.h"

namespace cali {

PythonAnnotation::PythonAnnotation(const char *name, cali_attr_properties opt)
    : cali::Annotation(name, opt) {}

PythonAnnotation &PythonAnnotation::begin() {
  cali::Annotation::begin();
  return *this;
}

PythonAnnotation &PythonAnnotation::begin(int data) {
  cali::Annotation::begin(data);
  return *this;
}

PythonAnnotation &PythonAnnotation::begin(double data) {
  cali::Annotation::begin(data);
  return *this;
}

PythonAnnotation &PythonAnnotation::begin(const char *data) {
  cali::Annotation::begin(data);
  return *this;
}

PythonAnnotation &PythonAnnotation::begin(cali_attr_type type,
                                          const std::string &data) {
  cali::Annotation::begin(type, data.data(), data.size());
  return *this;
}

PythonAnnotation &PythonAnnotation::begin(PythonVariant &data) {
  cali::Annotation::begin(data);
  return *this;
}

PythonAnnotation &PythonAnnotation::set(int data) {
  cali::Annotation::set(data);
  return *this;
}

PythonAnnotation &PythonAnnotation::set(double data) {
  cali::Annotation::set(data);
  return *this;
}

PythonAnnotation &PythonAnnotation::set(const char *data) {
  cali::Annotation::set(data);
  return *this;
}

PythonAnnotation &PythonAnnotation::set(cali_attr_type type,
                                        const std::string &data) {
  cali::Annotation::set(type, data.data(), data.size());
  return *this;
}

PythonAnnotation &PythonAnnotation::set(PythonVariant &data) {
  cali::Annotation::set(data);
  return *this;
}

void create_caliper_annotation_mod(py::module_ &caliper_annotation_mod) {
  py::class_<PythonAnnotation> annotation_type(caliper_annotation_mod,
                                               "Annotation");
  annotation_type.def(py::init<const char *, cali_attr_properties>(), "",
                      py::arg(), py::arg("opt") = CALI_ATTR_DEFAULT);
  annotation_type.def("end", &PythonAnnotation::end, "");
  annotation_type.def("begin",
                      static_cast<PythonAnnotation &(PythonAnnotation::*)()>(
                          &PythonAnnotation::begin),
                      "");
  annotation_type.def("begin",
                      static_cast<PythonAnnotation &(PythonAnnotation::*)(int)>(
                          &PythonAnnotation::begin),
                      "");
  annotation_type.def(
      "begin",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(const char *)>(
          &PythonAnnotation::begin),
      "");
  annotation_type.def(
      "begin",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(cali_attr_type,
                                                          const std::string &)>(
          &PythonAnnotation::begin),
      "");
  annotation_type.def(
      "begin",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(PythonVariant &)>(
          &PythonAnnotation::begin),
      "");
  annotation_type.def("set",
                      static_cast<PythonAnnotation &(PythonAnnotation::*)(int)>(
                          &PythonAnnotation::set),
                      "");
  annotation_type.def(
      "set",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(const char *)>(
          &PythonAnnotation::set),
      "");
  annotation_type.def(
      "set",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(cali_attr_type,
                                                          const std::string &)>(
          &PythonAnnotation::set),
      "");
  annotation_type.def(
      "set",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(PythonVariant &)>(
          &PythonAnnotation::set),
      "");
}

} // namespace cali
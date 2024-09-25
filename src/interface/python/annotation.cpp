#include "annotation.h"

namespace cali {

PythonAnnotation::PythonAnnotation(const char *name)
    : cali::Annotation(name, CALI_ATTR_DEFAULT) {}

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

void create_caliper_annotation_mod(py::module_ &caliper_annotation_mod) {
  py::class_<PythonAnnotation> annotation_type(caliper_annotation_mod,
                                               "Annotation");
  annotation_type.def(py::init<const char *>(),
                      "Creates an annotation object to manipulate the context "
                      "attribute with the given name.",
                      py::arg());
  annotation_type.def(py::init<const char *, cali_attr_properties>(),
                      "Creates an annotation object to manipulate the context "
                      "attribute with the given name",
                      py::arg(), py::arg("opt"));
  annotation_type.def(
      "end", &PythonAnnotation::end,
      "Close the top-most open region for the associated context attribute.");
  annotation_type.def("begin",
                      static_cast<PythonAnnotation &(PythonAnnotation::*)()>(
                          &PythonAnnotation::begin),
                      "Begin the region for the associated context attribute");
  annotation_type.def("begin",
                      static_cast<PythonAnnotation &(PythonAnnotation::*)(int)>(
                          &PythonAnnotation::begin),
                      "Begin the region for the associated context attribute "
                      "with an integer value");
  annotation_type.def(
      "begin",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(const char *)>(
          &PythonAnnotation::begin),
      "Begin the region for the associated context attribute "
      "with a str/bytes value");
  annotation_type.def(
      "begin",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(cali_attr_type,
                                                          const std::string &)>(
          &PythonAnnotation::begin),
      "Begin the region for the associated context attribute "
      "with a str/bytes value");
  annotation_type.def("set",
                      static_cast<PythonAnnotation &(PythonAnnotation::*)(int)>(
                          &PythonAnnotation::set),
                      "Exports an entry for the associated context attribute "
                      "with an integer value. The top-most prior open value "
                      "for the attribute, if any, will be overwritten.");
  annotation_type.def(
      "set",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(const char *)>(
          &PythonAnnotation::set),
      "Exports an entry for the associated context attribute "
      "with a str/bytes value. The top-most prior open value "
      "for the attribute, if any, will be overwritten.");
  annotation_type.def(
      "set",
      static_cast<PythonAnnotation &(PythonAnnotation::*)(cali_attr_type,
                                                          const std::string &)>(
          &PythonAnnotation::set),
      "Exports an entry for the associated context attribute "
      "with a str/bytes value. The top-most prior open value "
      "for the attribute, if any, will be overwritten.");
}

} // namespace cali
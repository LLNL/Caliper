#include "variant.h"
#include <stdexcept>

namespace cali {

PythonVariant::PythonVariant() : cali::Variant() {}

PythonVariant::PythonVariant(bool val) : cali::Variant(val) {}

PythonVariant::PythonVariant(int val) : cali::Variant(val) {}

PythonVariant::PythonVariant(double val) : cali::Variant(val) {}

PythonVariant::PythonVariant(unsigned int val) : cali::Variant(val) {}

PythonVariant::PythonVariant(const char *val) : cali::Variant(val) {}

PythonVariant::PythonVariant(cali_attr_type type, const std::string &data)
    : cali::Variant(type, data.data(), data.size()) {}

PythonVariant::PythonVariant(cali::Variant &&other) : cali::Variant(other) {}

int64_t PythonVariant::to_int() const {
  bool ok = true;
  int64_t ret = cali::Variant::to_int64(&ok);
  if (!ok) {
    throw std::runtime_error("Could not convert Variant to int");
  }
  return ret;
}

double PythonVariant::to_float() const {
  bool ok = true;
  double ret = cali::Variant::to_double(&ok);
  if (!ok) {
    throw std::runtime_error("Could not convert Variant to double");
  }
  return ret;
}

cali_attr_type PythonVariant::to_attr_type() {
  bool ok = true;
  cali_attr_type ret = cali::Variant::to_attr_type(&ok);
  if (!ok) {
    throw std::runtime_error(
        "Could not convert Variant to Caliper attribute type");
  }
  return ret;
}

py::bytes PythonVariant::pack() const {
  char buf[30];
  size_t real_size =
      cali::Variant::pack(reinterpret_cast<unsigned char *>(buf));
  std::string packed_variant{buf, real_size};
  return packed_variant;
}

PythonVariant PythonVariant::unpack(py::bytes packed_variant) {
  std::string cpp_packed_variant = packed_variant.cast<std::string>();
  bool ok = true;
  size_t variant_size = cpp_packed_variant.size();
  Variant unpacked_variant = cali::Variant::unpack(
      reinterpret_cast<const unsigned char *>(cpp_packed_variant.data()),
      &variant_size, &ok);
  if (!ok) {
    throw std::runtime_error("Could not unpack variant");
  }
  return unpacked_variant;
}

void create_caliper_variant_mod(py::module_ &caliper_variant_mod) {
  py::class_<PythonVariant> variant_type(caliper_variant_mod, "Variant");
  variant_type.def(py::init<>(), "Create default Variant");
  variant_type.def(py::init<bool>(), "Create boolean Variant");
  variant_type.def(py::init<int>(), "Create int Variant");
  variant_type.def(py::init<double>(), "Create double Variant");
  variant_type.def(py::init<unsigned int>(), "Create int Variant");
  variant_type.def(py::init<const char *>(), "Create string Variant");
  variant_type.def(py::init<cali_attr_type, const std::string &>(),
                   "Create custom Variant");
  variant_type.def("empty", &PythonVariant::empty, "Check if Variant is empty");
  variant_type.def("has_unmanaged_data", &PythonVariant::has_unmanaged_data,
                   "Check if Variant has unmanaged pointer data");
  variant_type.def_property_readonly("type", &PythonVariant::type,
                                     "The type of the Variant");
  variant_type.def_property_readonly("size", &PythonVariant::size,
                                     "The size of the Variant");
  variant_type.def("to_id", &PythonVariant::to_id,
                   "Get the Caliper ID for the Variant");
  variant_type.def(
      "to_int",
      [](const PythonVariant &variant) {
        int64_t int_val = variant.to_int();
        return int_val;
      },
      "Get the value of the Variant if it is an int");
  variant_type.def(
      "to_float",
      [](const PythonVariant &variant) {
        double float_val = variant.to_float();
        return float_val;
      },
      "Get the value of the Variant if it is a float");
  variant_type.def("to_attr_type",
                   static_cast<cali_attr_type (PythonVariant::*)()>(
                       &PythonVariant::to_attr_type),
                   "Get the type of the Variant");
  variant_type.def("to_string", &PythonVariant::to_string,
                   "Get the string value of the Variant");
  variant_type.def(
      "pack",
      [](const PythonVariant &variant) {
        py::bytes packed_data = variant.pack();
        return packed_data;
      },
      "Pack the Variant into a byte buffer");
  variant_type.def_static("unpack", &PythonVariant::unpack,
                          "Create a Variant from a byte buffer");
  variant_type.def("__eq__", [](const PythonVariant &v1,
                                const PythonVariant &v2) { return v1 == v2; });
  variant_type.def("__ne__", [](const PythonVariant &v1,
                                const PythonVariant &v2) { return v1 != v2; });
  variant_type.def("__lt__", [](const PythonVariant &v1,
                                const PythonVariant &v2) { return v1 < v2; });
  variant_type.def("__gt__", [](const PythonVariant &v1,
                                const PythonVariant &v2) { return v1 > v2; });
}

} // namespace cali
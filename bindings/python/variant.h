#ifndef CALI_INTERFACE_PYTHON_VARIANT_H
#define CALI_INTERFACE_PYTHON_VARIANT_H

#include "common.h"

namespace cali {

class PythonVariant : public cali::Variant {
public:
  PythonVariant();

  PythonVariant(bool val);

  PythonVariant(int val);

  PythonVariant(double val);

  PythonVariant(unsigned int val);

  PythonVariant(const char *val);

  PythonVariant(cali_attr_type type, const std::string &data);

  int64_t to_int() const;

  double to_float() const;

  cali_attr_type to_attr_type();

  py::bytes pack() const;

  static PythonVariant unpack(py::bytes packed_variant);

private:
  PythonVariant(Variant &&other);
};

void create_caliper_variant_mod(py::module_ &caliper_variant_mod);

} // namespace cali

#endif /* CALI_INTERFACE_PYTHON_VARIANT_H */
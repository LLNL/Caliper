#ifndef CALI_INTERFACE_PYTHON_INSTRUMENTATION_H
#define CALI_INTERFACE_PYTHON_INSTRUMENTATION_H

#include "common.h"

namespace cali {

class PythonAttribute {
public:
  PythonAttribute(const char *name, cali_attr_type type,
                  cali_attr_properties opt = CALI_ATTR_DEFAULT);

  static PythonAttribute find_attribute(const char *name);

  const char *name() const;

  cali_attr_type type() const;

  cali_attr_properties properties() const;

  void begin();

  void begin(int val);

  void begin(double val);

  void begin(const char *val);

  void set(int val);

  void set(double val);

  void set(const char *val);

  void end();

private:
  PythonAttribute(cali_id_t id);

  cali_id_t m_attr_id;
};

// TODO add "byname" functions to module

void create_caliper_instrumentation_mod(
    py::module_ &caliper_instrumentation_mod);

} // namespace cali

#endif /* CALI_INTERFACE_PYTHON_INSTRUMENTATION_H */
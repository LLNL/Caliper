#ifndef CALI_INTERFACE_PYTHON_ANNOTATION_H
#define CALI_INTERFACE_PYTHON_ANNOTATION_H

#include "variant.h"

namespace cali {

class PythonAnnotation : public cali::Annotation {
public:
  PythonAnnotation(const char *name,
                   cali_attr_properties opt = CALI_ATTR_DEFAULT);

  PythonAnnotation &begin();

  PythonAnnotation &begin(int data);

  PythonAnnotation &begin(double data);

  PythonAnnotation &begin(const char *data);

  PythonAnnotation &begin(cali_attr_type type, const std::string &data);

  PythonAnnotation &begin(PythonVariant &data);

  PythonAnnotation &set(int data);

  PythonAnnotation &set(double data);

  PythonAnnotation &set(const char *data);

  PythonAnnotation &set(cali_attr_type type, const std::string &data);

  PythonAnnotation &set(PythonVariant &data);
};

void create_caliper_annotation_mod(py::module_ &caliper_annotation_mod);

} // namespace cali

#endif /* CALI_INTERFACE_PYTHON_ANNOTATION_H */
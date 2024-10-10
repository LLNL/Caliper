#ifndef CALI_INTERFACE_PYTHON_LOOP_H
#define CALI_INTERFACE_PYTHON_LOOP_H

#include "common.h"

namespace cali
{

class PythonLoop
{
public:

    PythonLoop(const char* name);

    void start_iteration(int i);

    void end_iteration();

    void end();

private:

    cali_id_t m_iter_attr;
};

void create_caliper_loop_mod(py::module_& caliper_loop_mod);

} // namespace cali

#endif /* CALI_INTERFACE_PYTHON_LOOP_H */
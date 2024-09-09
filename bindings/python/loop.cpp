#include "loop.h"

namespace cali {

PythonLoop::PythonLoop(const char *name) {
  if (cali_loop_attr_id == CALI_INV_ID) {
    cali_init();
  }
  cali_begin_string(cali_loop_attr_id, name);
  m_iter_attr = cali_make_loop_iteration_attribute(name);
}

void PythonLoop::start_iteration(int i) { cali_begin_int(m_iter_attr, i); }

void PythonLoop::end_iteration() { cali_end(m_iter_attr); }

void PythonLoop::end() { cali_end(cali_loop_attr_id); }

void create_caliper_loop_mod(py::module_ &caliper_loop_mod) {
  py::class_<PythonLoop> loop_type(caliper_loop_mod, "Loop");
  loop_type.def(py::init<const char *>(), "Create a loop annotation.");
  loop_type.def("start_iteration", &PythonLoop::start_iteration,
                "Start a loop iteration.");
  loop_type.def("end_iteration", &PythonLoop::end_iteration,
                "End a loop iteration.");
  loop_type.def("end", &PythonLoop::end, "End the loop annotation.");
}

} // namespace cali
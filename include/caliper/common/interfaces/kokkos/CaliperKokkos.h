#include "impl/Kokkos_Profiling_Interface.hpp"

namespace cali {

namespace kokkos {

using Kokkos::Profiling::Experimental::EventSet;

enum ProfilingComponents {
  fors = 1,
  reduces = 2,
  scans = 4,
  copies = 8,
  regions = 16,
  all = 31
};

EventSet get_event_set(size_t components);

}
}

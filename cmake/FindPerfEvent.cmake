include(CheckIncludeFiles)

set(PERF_PARANOID /proc/sys/kernel/perf_event_paranoid)
if(EXISTS ${PERF_PARANOID})
  set(HAVE_PERF_PARANOID TRUE)
else()
  message(ERROR "${PERF_PARANOID} not found, perf_event service requires a kernel with perf_events enabled!")
  set(HAVE_PERF_PARANOID FALSE)
endif()

check_include_files(linux/perf_event.h HAVE_PERF_EVENT_H)

find_package_handle_standard_args(PerfEvent
  FAIL_MESSAGE "ERROR: Need a kernel with perf_event support."
  REQUIRED_VARS HAVE_PERF_PARANOID HAVE_PERF_EVENT_H)


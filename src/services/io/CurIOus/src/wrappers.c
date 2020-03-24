#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "wrappers.h"
#include "callbacks.h"
#include "file_registry.h"
#include "mount_tree.h"

/*********************
 * Wrapper Functions *
 *********************/

// Typedefs just for easy conversion pf original functions
typedef ssize_t (*read_f)(int fd, void *buf, size_t count);
typedef ssize_t (*write_f)(int fd, const void *buf, size_t count);
typedef int (*open_f)(const char *pathname, int flags, mode_t mode);
typedef int (*close_f)(int fd);
typedef int (*stat_f)(const char *path, struct stat *buf);
typedef int (*xstat_f)(int vers, const char *path, struct stat *buf);
typedef int (*fstat_f)(int fd, struct stat *buf);
typedef int (*fxstat_f)(int vers, int fd, struct stat *buf);
typedef FILE * (*fopen_f)(const char *path, const char *mode);
typedef FILE * (*fdopen_f)(int fd, const char *mode);
typedef FILE * (*freopen_f)(const char *path, const char *mode, FILE *stream);
typedef int (*fclose_f)(FILE *fp);
typedef int (*vprintf_f)(const char *format, va_list ap);
typedef int (*vfprintf_f)(FILE *stream, const char *format, va_list ap);
typedef int (*vscanf_f)(const char *format, va_list ap);
typedef int (*vfscanf_f)(FILE *stream, const char *format, va_list ap);
typedef int (*fgetc_f)(FILE *stream);
typedef char * (*fgets_f)(char *s, int size, FILE *stream);
typedef int (*getchar_f)(void);
typedef char * (*gets_f)(char *s);
typedef size_t (*fread_f)(void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef size_t (*fwrite_f)(void *ptr, size_t size, size_t nmemb, FILE *stream);

// Actual declarations of wrappers

// POSIX Wrappers:
static ssize_t wrapped_read(int fd, void *buf, size_t count);
static ssize_t wrapped_write(int fd, const void *buf, size_t count);
static int wrapped_open(const char *pathname, int flags, mode_t mode);
static int wrapped_close(int fd);
static int wrapped_stat(const char *path, struct stat *buf);
static int wrapped_xstat(int vers, const char *path, struct stat *buf);
static int wrapped_xstat64(int vers, const char *path, struct stat *buf);
static int wrapped_lstat(const char *path, struct stat *buf);
static int wrapped_lxstat(int vers, const char *path, struct stat *buf);
static int wrapped_lxstat64(int vers, const char *path, struct stat *buf);
static int wrapped_fstat(int fd, struct stat *buf);
static int wrapped_fxstat(int vers, int fd, struct stat *buf);
static int wrapped_fxstat64(int vers, int fd, struct stat *buf);

// C stdio Wrappers:
static FILE * wrapped_fopen(const char *path, const char *mode);
static FILE * wrapped_fdopen(int fd, const char *mode);
static FILE * wrapped_freopen(const char *path, const char *mode, FILE *stream);
static int wrapped_fclose(FILE *fp);
static int wrapped_printf(const char *format, ...);
static int wrapped_fprintf(FILE *stream, const char *format, ...);
static int wrapped_vprintf(const char *format, va_list ap);
static int wrapped_vfprintf(FILE *stream, const char *format, va_list ap);
/*
int wrapped_scanf(const char *format, ...);
int wrapped_fscanf(FILE *stream, const char *format, ...);
int wrapped_vscanf(const char *format, va_list ap);
int wrapped_vfscanf(FILE *stream, const char *format, va_list ap);
*/
static int wrapped_fgetc(FILE *stream);
static char * wrapped_fgets(char *s, int size, FILE *stream);
static int wrapped_getchar(void);
//char * wrapped_gets(char *s);
static size_t wrapped_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
static size_t wrapped_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream);
  
// Note that the handles need not be intialised, just defined
io_function_data_t io_fns[CURIOUS_FUNCTION_COUNT] = {
  [CURIOUS_READ_ID]       = { CURIOUS_POSIX, 0 },
  [CURIOUS_WRITE_ID]      = { CURIOUS_POSIX, 0  },
  [CURIOUS_OPEN_ID]       = { CURIOUS_POSIX, 0  },
  [CURIOUS_CLOSE_ID]      = { CURIOUS_POSIX, 0  },
  [CURIOUS_STAT_ID]       = { CURIOUS_POSIX, 0  },
  [CURIOUS__STAT_ID]      = { CURIOUS_POSIX, 0  },
  [CURIOUS_LSTAT_ID]      = { CURIOUS_POSIX, 0  },
  [CURIOUS__LSTAT_ID]     = { CURIOUS_POSIX, 0  },
  [CURIOUS_XSTAT_ID]      = { CURIOUS_POSIX, 0  },
  [CURIOUS__XSTAT_ID]     = { CURIOUS_POSIX, 0  },
  [CURIOUS_XSTAT64_ID]    = { CURIOUS_POSIX, 0  },
  [CURIOUS__XSTAT64_ID]   = { CURIOUS_POSIX, 0  },
  [CURIOUS_LXSTAT_ID]     = { CURIOUS_POSIX, 0  },
  [CURIOUS__LXSTAT_ID]    = { CURIOUS_POSIX, 0  },
  [CURIOUS_LXSTAT64_ID]   = { CURIOUS_POSIX, 0  },
  [CURIOUS__LXSTAT64_ID]  = { CURIOUS_POSIX, 0  },
  [CURIOUS_FSTAT_ID]      = { CURIOUS_POSIX, 0  },
  [CURIOUS__FSTAT_ID]     = { CURIOUS_POSIX, 0  },
  [CURIOUS_FXSTAT_ID]     = { CURIOUS_POSIX, 0  },
  [CURIOUS__FXSTAT_ID]    = { CURIOUS_POSIX, 0  },
  [CURIOUS_FXSTAT64_ID]   = { CURIOUS_POSIX, 0  },
  [CURIOUS__FXSTAT64_ID]  = { CURIOUS_POSIX, 0  },
  [CURIOUS_FOPEN_ID]      = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_FOPEN64_ID]    = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_FDOPEN_ID]     = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_FREOPEN_ID]    = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_FCLOSE_ID]     = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_PRINTF_ID]     = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_FPRINTF_ID]    = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_VPRINTF_ID]    = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_VFPRINTF_ID]   = { CURIOUS_CSTDIO, 0 },
  /*
  [CURIOUS_SCANF_ID]      = { CURIOUS_CSTDIO },
  [CURIOUS_FSCANF_ID]     = { CURIOUS_CSTDIO },
  [CURIOUS_VSCANF_ID]     = { CURIOUS_CSTDIO },
  [CURIOUS_VFSCANF_ID]    = { CURIOUS_CSTDIO },
  */
  [CURIOUS_FGETC_ID]      = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_FGETS_ID]      = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_GETCHAR_ID]    = { CURIOUS_CSTDIO, 0 },
//[CURIOUS_GETS_ID]       = { CURIOUS_CSTDIO },
  [CURIOUS_FREAD_ID]      = { CURIOUS_CSTDIO, 0 },
  [CURIOUS_FWRITE_ID]     = { CURIOUS_CSTDIO, 0 },
};

// Just a shortcut to get the bindings right for all the functions
#define GET_BINDING(lower, upper) \
  { #lower, (void *) wrapped_##lower, &io_fns[CURIOUS_##upper##_ID].handle }

static gotcha_binding_t bindings[CURIOUS_FUNCTION_COUNT] = {
  [CURIOUS_READ_ID]       = GET_BINDING(read,READ),
// GET_BINDING(read,READ), expands to { "read", read_wrapper, &io_fns[CURIOUS_READ_ID].handle }
  [CURIOUS_WRITE_ID]      = GET_BINDING(write,WRITE),
  [CURIOUS_OPEN_ID]       = GET_BINDING(open,OPEN),
  [CURIOUS_CLOSE_ID]      = GET_BINDING(close,CLOSE),
  [CURIOUS_STAT_ID]       = GET_BINDING(stat,STAT),
  [CURIOUS__STAT_ID]      = { "__stat", (void *) wrapped_stat, &io_fns[CURIOUS_STAT_ID].handle },
  [CURIOUS_LSTAT_ID]      = GET_BINDING(lstat,LSTAT),
  [CURIOUS__LSTAT_ID]     = { "__lstat", (void *) wrapped_lstat, &io_fns[CURIOUS_LSTAT_ID].handle },
  [CURIOUS_XSTAT_ID]      = GET_BINDING(xstat,XSTAT),
  [CURIOUS__XSTAT_ID]     = { "__xstat", (void *) wrapped_xstat, &io_fns[CURIOUS_XSTAT_ID].handle },
  [CURIOUS_XSTAT64_ID]    = GET_BINDING(xstat64,XSTAT64),
  [CURIOUS__XSTAT64_ID]   = { "__xstat64", (void *) wrapped_xstat64, &io_fns[CURIOUS_XSTAT64_ID].handle },
  [CURIOUS_LXSTAT_ID]     = GET_BINDING(lxstat,LXSTAT),
  [CURIOUS__LXSTAT_ID]    = { "__lxstat", (void *) wrapped_lxstat, &io_fns[CURIOUS_LXSTAT_ID].handle },
  [CURIOUS_LXSTAT64_ID]   = GET_BINDING(lxstat64,LXSTAT64),
  [CURIOUS__LXSTAT64_ID]  = { "__lxstat64", (void *) wrapped_lxstat64, &io_fns[CURIOUS_LXSTAT64_ID].handle },
  [CURIOUS_FSTAT_ID]      = GET_BINDING(fstat,FSTAT),
  [CURIOUS__FSTAT_ID]     = { "__fstat", (void *) wrapped_fstat, &io_fns[CURIOUS_FSTAT_ID].handle },
  [CURIOUS_FXSTAT_ID]     = GET_BINDING(fxstat,FXSTAT),
  [CURIOUS__FXSTAT_ID]    = { "__fxstat", (void *) wrapped_fxstat, &io_fns[CURIOUS_FXSTAT_ID].handle },
  [CURIOUS_FXSTAT64_ID]   = GET_BINDING(fxstat64,FXSTAT64),
  [CURIOUS__FXSTAT64_ID]  = { "__fxstat64", (void *) wrapped_fxstat64, &io_fns[CURIOUS_FXSTAT64_ID].handle },
  [CURIOUS_FOPEN_ID]      = GET_BINDING(fopen,FOPEN),
  [CURIOUS_FOPEN64_ID]    = { "fopen64", (void *) wrapped_fopen, &io_fns[CURIOUS_FOPEN64_ID].handle },
  [CURIOUS_FDOPEN_ID]     = GET_BINDING(fdopen,FDOPEN),
  [CURIOUS_FREOPEN_ID]    = GET_BINDING(freopen,FREOPEN),
  [CURIOUS_FCLOSE_ID]     = GET_BINDING(fclose,FCLOSE),
  [CURIOUS_PRINTF_ID]     = GET_BINDING(printf,PRINTF),
  [CURIOUS_FPRINTF_ID]    = GET_BINDING(fprintf,FPRINTF),
  [CURIOUS_VPRINTF_ID]    = GET_BINDING(vprintf,VPRINTF),
  [CURIOUS_VFPRINTF_ID]   = GET_BINDING(vfprintf,VFPRINTF),
  /*
  [CURIOUS_SCANF_ID]      = GET_BINDING(scanf,SCANF),
  [CURIOUS_FSCANF_ID]     = GET_BINDING(fscanf,FSCANF),
  [CURIOUS_VSCANF_ID]     = GET_BINDING(vscanf,VSCANF),
  [CURIOUS_VFSCANF_ID]    = GET_BINDING(vfscanf,VFSCANF),
  */
  [CURIOUS_FGETC_ID]      = GET_BINDING(fgetc,FGETC),
  [CURIOUS_FGETS_ID]      = GET_BINDING(fgets,FGETS),
  [CURIOUS_GETCHAR_ID]    = GET_BINDING(getchar,GETCHAR),
//[CURIOUS_GETS_ID]       = GET_BINDING(gets,GETS),
  [CURIOUS_FREAD_ID]      = GET_BINDING(fread,FREAD),
  [CURIOUS_FWRITE_ID]     = GET_BINDING(fwrite,FWRITE),
};

// Just a boolean to control whether or not wrappers do anything or not
static volatile sig_atomic_t wrappers_enabled;

void curious_apply_wrappers(curious_apis_t apis) {
  if (apis & CURIOUS_POSIX) {
    gotcha_wrap(&bindings[CURIOUS_POSIX_START], CURIOUS_CSTDIO_START - CURIOUS_POSIX_START, "curious");
  }
  if (apis & CURIOUS_CSTDIO) {
    gotcha_wrap(&bindings[CURIOUS_CSTDIO_START], CURIOUS_FUNCTION_COUNT - CURIOUS_CSTDIO_START, "curious");
  }

  wrappers_enabled = 1;
}

void curious_disable_wrappers(void) {
  wrappers_enabled = 0;
}

// Keeps track of how many wrapper calls deep each thread is;
// Allows us to avoid calling wrappers on functions called in the wrappers
#ifdef _Thread_local
static _Thread_local volatile sig_atomic_t wrapper_call_depth;
#else
static __thread volatile sig_atomic_t wrapper_call_depth;
#endif

#define WRAPPER_ENTER(wrapper) \
  wrapper_call_depth++; \
  /* printf("entering %s wrapper\n", #wrapper); */

// Just to make sure we decremint call depth before returning
#define WRAPPER_RETURN(return_val, wrapper) \
  /* printf("exiting %s wrapper\n", #wrapper); */ \
  wrapper_call_depth--; \
  return return_val;

ssize_t wrapped_read(int fd, void *buf, size_t count) {
  WRAPPER_ENTER(read);

  // Get the handle for the original function
  read_f orig_read = (read_f) gotcha_get_wrappee(io_fns[CURIOUS_READ_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fd);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_read_record_t io_args = {
      .bytes_read = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_READ_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    io_args.bytes_read = orig_read(fd, buf, count);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(io_args.bytes_read, read);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_read(fd, buf, count), read);
  }
}

ssize_t wrapped_write(int fd, const void *buf, size_t count) {
  WRAPPER_ENTER(write);

  // Get the handle for the original function
  write_f orig_write = (write_f) gotcha_get_wrappee(io_fns[CURIOUS_WRITE_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fd);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_write_record_t io_args = {
      .bytes_written = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_WRITE_ID,
    };
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK, &io_args);

    // Call the original, saving the result...
    io_args.bytes_written = orig_write(fd, buf, count);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(io_args.bytes_written, write);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_write(fd, buf, count), write);
  }
}

int wrapped_open(const char *pathname, int flags, mode_t mode) {
  WRAPPER_ENTER(open);

  curious_metadata_record_t io_args = {
    .call_count = 0,
    .function_id = CURIOUS_OPEN_ID,
  };

  if (1 == wrapper_call_depth && wrappers_enabled) {
    // NOTE: calling realpath on pathname before passing it to curious_get_filesystem
    //       might be necessary in order to handle relative paths_

    curious_get_filesystem(pathname, &io_args.filesystem, &io_args.mount_point);
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);
  }

  // Get the handle for the original function
  open_f orig_open = (open_f) gotcha_get_wrappee(io_fns[CURIOUS_OPEN_ID].handle);

  // Call the original, saving the result...
  int fd = orig_open(pathname, flags, mode);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_register_file_by_fd(fd);
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);
  }

  WRAPPER_RETURN(fd, open);
}

int wrapped_close(int fd) {
  WRAPPER_ENTER(close);

  // Get the handle for the original function
  close_f orig_close = (close_f) gotcha_get_wrappee(io_fns[CURIOUS_CLOSE_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fd);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_metadata_record_t io_args = {
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_CLOSE_ID,
    };
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_close(fd);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);


    WRAPPER_RETURN(return_val, close);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_close(fd), close);
  }
}

int wrapped_stat(const char *path, struct stat *buf) {
  WRAPPER_ENTER(stat);

  // Get the handle for the original function
  stat_f orig_stat = (stat_f) gotcha_get_wrappee(io_fns[CURIOUS_STAT_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_metadata_record_t io_args = {
      .call_count = 0,
      .function_id = CURIOUS_STAT_ID,
    };

    curious_get_filesystem(path, &io_args.filesystem, &io_args.mount_point);

    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_stat(path, buf);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, stat);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_stat(path, buf), stat);
  }
}

int wrapped_lstat(const char *path, struct stat *buf) {
  WRAPPER_ENTER(lstat);

  // Get the handle for the original function
  stat_f orig_lstat = (stat_f) gotcha_get_wrappee(io_fns[CURIOUS_LSTAT_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_metadata_record_t io_args = {
      .call_count = 0,
      .function_id = CURIOUS_LSTAT_ID,
    };

    curious_get_filesystem(path, &io_args.filesystem, &io_args.mount_point);

    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_lstat(path, buf);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, lstat);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_lstat(path, buf), lstat);
  }
}

int wrapped_xstat(int vers, const char *path, struct stat *buf) {
  WRAPPER_ENTER(xstat);

  // Get the handle for the original function
  xstat_f orig_xstat = (xstat_f) gotcha_get_wrappee(io_fns[CURIOUS_XSTAT_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_metadata_record_t io_args = {
      .call_count = 0,
      .function_id = CURIOUS_XSTAT_ID,
    };

    curious_get_filesystem(path, &io_args.filesystem, &io_args.mount_point);

    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_xstat(vers, path, buf);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, xstat);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_xstat(vers, path, buf), xstat);
  }
}

int wrapped_xstat64(int vers, const char *path, struct stat *buf) {
  WRAPPER_ENTER(xstat64);

  // Get the handle for the original function
  xstat_f orig_xstat64 = (xstat_f) gotcha_get_wrappee(io_fns[CURIOUS_XSTAT64_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_metadata_record_t io_args = {
      .call_count = 0,
      .function_id = CURIOUS_XSTAT64_ID,
    };

    curious_get_filesystem(path, &io_args.filesystem, &io_args.mount_point);

    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_xstat64(vers, path, buf);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, xstat64);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_xstat64(vers, path, buf), xstat64);
  }
}

int wrapped_lxstat(int vers, const char *path, struct stat *buf) {
  WRAPPER_ENTER(lxstat);

  // Get the handle for the original function
  xstat_f orig_lxstat = (xstat_f) gotcha_get_wrappee(io_fns[CURIOUS_LXSTAT_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_metadata_record_t io_args = {
      .call_count = 0,
      .function_id = CURIOUS_LXSTAT_ID,
    };

    curious_get_filesystem(path, &io_args.filesystem, &io_args.mount_point);

    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_lxstat(vers, path, buf);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, lxstat);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_lxstat(vers, path, buf), lxstat);
  }
}

int wrapped_lxstat64(int vers, const char *path, struct stat *buf) {
  WRAPPER_ENTER(xstat);

  // Get the handle for the original function
  xstat_f orig_lxstat64 = (xstat_f) gotcha_get_wrappee(io_fns[CURIOUS_XSTAT_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_metadata_record_t io_args = {
      .call_count = 0,
      .function_id = CURIOUS_LXSTAT64_ID,
    };

    curious_get_filesystem(path, &io_args.filesystem, &io_args.mount_point);

    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_lxstat64(vers, path, buf);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, lxstat64);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_lxstat64(vers, path, buf), lxstat64);
  }
}

int wrapped_fstat(int fd, struct stat *buf) {
  WRAPPER_ENTER(fstat);

  // Get the handle for the original function
  fstat_f orig_fstat = (fstat_f) gotcha_get_wrappee(io_fns[CURIOUS_FSTAT_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fd);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_metadata_record_t io_args = {
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FSTAT_ID,
    };
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_fstat(fd, buf);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, fstat);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_fstat(fd, buf), fstat);
  }
}

int wrapped_fxstat(int vers, int fd, struct stat *buf) {
  WRAPPER_ENTER(fxstat);

  // Get the handle for the original function
  fxstat_f orig_fxstat = (fxstat_f) gotcha_get_wrappee(io_fns[CURIOUS_FXSTAT_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fd);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_metadata_record_t io_args = {
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FXSTAT_ID,
    };
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_fxstat(vers, fd, buf);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, fxstat);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_fxstat(vers, fd, buf), fxstat);
  }
}

int wrapped_fxstat64(int vers, int fd, struct stat *buf) {
  WRAPPER_ENTER(fxstat64);

  // Get the handle for the original function
  fxstat_f orig_fxstat64 = (fxstat_f) gotcha_get_wrappee(io_fns[CURIOUS_FXSTAT64_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fd);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_metadata_record_t io_args = {
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FXSTAT64_ID,
    };
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_fxstat64(vers, fd, buf);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, fxstat64);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_fxstat64(vers, fd, buf), fxstat64);
  }
}

FILE * wrapped_fopen(const char *path, const char *mode) {
  WRAPPER_ENTER(fopen);

  // Get the handle for the original function
  fopen_f orig_fopen = (fopen_f) gotcha_get_wrappee(io_fns[CURIOUS_FOPEN_ID].handle);

  // NOTE: calling realpath on pathname before passing it to curious_get_filesystem
  //       might be necessary in order to handle relative paths_

  curious_metadata_record_t io_args = {
    .call_count = 0,
    .function_id = CURIOUS_FOPEN_ID,
  };

  curious_get_filesystem(path, &io_args.filesystem, &io_args.mount_point);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);
  }

  // Call the original, saving the result...
  FILE *file = orig_fopen(path, mode);

  if (NULL != file) {
    curious_register_file_by_fd(fileno(file));
  }

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);
  }

  WRAPPER_RETURN(file, fopen);
}

FILE * wrapped_fdopen(int fd, const char *mode) {
  WRAPPER_ENTER(fdopen);

  // Get the handle for the original function
  fdopen_f orig_fdopen = (fdopen_f) gotcha_get_wrappee(io_fns[CURIOUS_FDOPEN_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fd);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_metadata_record_t io_args = {
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FDOPEN_ID,
    };
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    FILE *file = orig_fdopen(fd, mode);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(file, fdopen);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_fdopen(fd, mode), fdopen);
  }
}

FILE * wrapped_freopen(const char *path, const char *mode, FILE *stream) {
  WRAPPER_ENTER(freopen);

  // Get the handle for the original function
  freopen_f orig_freopen = (freopen_f) gotcha_get_wrappee(io_fns[CURIOUS_FREOPEN_ID].handle);

  // NOTE: calling realpath on pathname before passing it to curious_get_filesystem
  //       might be necessary in order to handle relative paths_

  curious_metadata_record_t io_args = {
    .call_count = 0,
    .function_id = CURIOUS_FREOPEN_ID,
  };

  curious_get_filesystem(path, &io_args.filesystem, &io_args.mount_point);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);
  }

  // Call the original, saving the result...
  FILE *file = orig_freopen(path, mode, stream);

  if (NULL != file) {
    curious_register_file_by_fd(fileno(file));
  }
  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);
  }

  WRAPPER_RETURN(file, freopen);
}

int wrapped_fclose(FILE *fp) {
  WRAPPER_ENTER(fclose);

  // Get the handle for the original function
  fclose_f orig_fclose = (fclose_f) gotcha_get_wrappee(io_fns[CURIOUS_FCLOSE_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fileno(fp));
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_deregister_file(fileno(fp));

    curious_metadata_record_t io_args = {
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FCLOSE_ID,
    };
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_fclose(fp);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_METADATA_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);


    WRAPPER_RETURN(return_val, fclose);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_fclose(fp), fclose);
  }
}

int wrapped_printf(const char *format, ...) {
  WRAPPER_ENTER(printf);

  // Get the handle for the original function
  vprintf_f orig_vprintf = (vprintf_f) gotcha_get_wrappee(io_fns[CURIOUS_VPRINTF_ID].handle);

  // Get the variable-length argument list set up
  va_list args;
  va_start(args, format);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(STDOUT_FILENO);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_write_record_t io_args = {
      .bytes_written = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_PRINTF_ID,
    };
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_vprintf(format, args);
    io_args.call_count = 1;
    if (return_val > 0) {
      io_args.bytes_written = return_val;
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, printf);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_vprintf(format, args), printf);
  }
}

int wrapped_fprintf(FILE *stream, const char *format, ...) {
  WRAPPER_ENTER(fprintf);

  // Get the handle for the original function
  vfprintf_f orig_vfprintf = (vfprintf_f) gotcha_get_wrappee(io_fns[CURIOUS_VFPRINTF_ID].handle);

  // Get the variable-length argument list set up
  va_list args;
  va_start(args, format);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fileno(stream));
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_write_record_t io_args = {
      .bytes_written = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FPRINTF_ID,
    };
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_vfprintf(stream, format, args);
    io_args.call_count = 1;
    if (return_val > 0) {
      io_args.bytes_written = return_val;
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, fprintf);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_vfprintf(stream, format, args), fprintf);
  }
}

int wrapped_vprintf(const char *format, va_list ap) {
  WRAPPER_ENTER(vprintf);

  // Get the handle for the original function
  vprintf_f orig_vprintf = (vprintf_f) gotcha_get_wrappee(io_fns[CURIOUS_VPRINTF_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(STDOUT_FILENO);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_write_record_t io_args = {
      .bytes_written = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_VPRINTF_ID,
    };
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_vprintf(format, ap);
    io_args.call_count = 1;
    if (return_val > 0) {
      io_args.bytes_written = return_val;
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, vprintf);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_vprintf(format, ap), vprintf);
  }
}

int wrapped_vfprintf(FILE *stream, const char *format, va_list ap) {
  WRAPPER_ENTER(vfprintf);

  // Get the handle for the original function
  vfprintf_f orig_vfprintf = (vfprintf_f) gotcha_get_wrappee(io_fns[CURIOUS_VFPRINTF_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fileno(stream));
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_write_record_t io_args = {
      .bytes_written = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_VFPRINTF_ID,
    };
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_vfprintf(stream, format, ap);
    io_args.call_count = 1;
    if (return_val > 0) {
      io_args.bytes_written = return_val;
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, vfprintf);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_vfprintf(stream, format, ap), vfprintf);
  }
}

/*
int wrapped_scanf(const char *format, ...) {
  WRAPPER_ENTER(scanf);

  // Get the handle for the original function
  vscanf_f orig_vscanf = (vscanf_f) gotcha_get_wrappee(io_fns[CURIOUS_VSCANF_ID].handle);

  // Get the variable-length argument list set up
  va_list args;
  va_start(args, format);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    curious_file_record_t *record = get_curious_file_record(STDIN_FILENO);
    if (NULL == record) {
      filesystem = NULL;
    } else {
      filesystem = record->filesystem;
    }

    curious_read_record_t io_args = {
      .bytes_read = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .function_id = CURIOUS_SCANF_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_vscanf(format, args);
    io_args.call_count = 1;
    // TODO: figure out how many bytes read
    //       sounds like a messy solution would involve adding "%n"
    //       at the end of the format string and adding an int buffer
    //       to the va_list passed to orig_vscanf

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, scanf);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_vscanf(format, args), scanf);
  }
}

int wrapped_fscanf(FILE *stream, const char *format, ...) {
  WRAPPER_ENTER(fscanf);

  // Get the handle for the original function
  vfscanf_f orig_vfscanf = (vfscanf_f) gotcha_get_wrappee(io_fns[CURIOUS_VFSCANF_ID].handle);

  // Get the variable-length argument list set up
  va_list args;
  va_start(args, format);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    curious_file_record_t *record = get_curious_file_record(fileno(stream));
    if (NULL == record) {
      filesystem = NULL;
    } else {
      filesystem = record->filesystem;
    }

    curious_read_record_t io_args = {
      .call_count = 0,
      .filesystem = filesystem,
      .function_id = CURIOUS_FSCANF_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_vfscanf(stream, format, args);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, fscanf);

  // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_vfscanf(stream, format, args), fscanf);
  }
}

int wrapped_vscanf(const char *format, va_list ap) {
  WRAPPER_ENTER(vscanf);

  // Get the handle for the original function
  vscanf_f orig_vscanf = (vscanf_f) gotcha_get_wrappee(io_fns[CURIOUS_VSCANF_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    curious_file_record_t *record = get_curious_file_record(STDOUT_FILENO);
    if (NULL == record) {
      filesystem = NULL;
    } else {
      filesystem = record->filesystem;
    }

    curious_read_record_t io_args = {
      .call_count = 0,
      .filesystem = filesystem,
      .function_id = CURIOUS_VSCANF_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_vscanf(format, ap);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, vscanf);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_vscanf(format, ap), vscanf);
  }
}

int wrapped_vfscanf(FILE *stream, const char *format, va_list ap) {
  WRAPPER_ENTER(vfscanf);

  // Get the handle for the original function
  vfscanf_f orig_vfscanf = (vfscanf_f) gotcha_get_wrappee(io_fns[CURIOUS_VFSCANF_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    curious_file_record_t *record = get_curious_file_record(fileno(stream));
    if (NULL == record) {
      filesystem = NULL;
    } else {
      filesystem = record->filesystem;
    }

    curious_read_record_t io_args = {
      .call_count = 0,
      .filesystem = filesystem,
      .function_id = CURIOUS_VFSCANF_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_vfscanf(stream, format, ap);
    io_args.call_count = 1;

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, vfscanf);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_vfscanf(stream, format, ap), vfscanf);
  }
}
*/

int wrapped_fgetc(FILE *stream) {
  WRAPPER_ENTER(fgetc);

  // Get the handle for the original function
  fgetc_f orig_fgetc = (fgetc_f) gotcha_get_wrappee(io_fns[CURIOUS_FGETC_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fileno(stream));
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_read_record_t io_args = {
      .bytes_read = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FGETC_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_fgetc(stream);
    io_args.call_count = 1;
    if (EOF != return_val) {
      io_args.bytes_read = 1;
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, fgetc);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_fgetc(stream), fgetc);
  }
}

char * wrapped_fgets(char *s, int size, FILE *stream) {
  WRAPPER_ENTER(fgets);

  // Get the handle for the original function
  fgets_f orig_fgets = (fgets_f) gotcha_get_wrappee(io_fns[CURIOUS_FGETS_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fileno(stream));
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_read_record_t io_args = {
      .bytes_read = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FGETS_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    char *return_val = orig_fgets(s, size, stream);
    io_args.call_count = 1;
    if (NULL != return_val) {
      io_args.bytes_read = strlen(return_val);
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, fgets);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_fgets(s, size, stream), fgets);
  }
}

int wrapped_getchar(void) {
  WRAPPER_ENTER(getchar);

  // Get the handle for the original function
  getchar_f orig_getchar = (getchar_f) gotcha_get_wrappee(io_fns[CURIOUS_GETCHAR_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(STDIN_FILENO);
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_read_record_t io_args = {
      .bytes_read = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_GETCHAR_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    int return_val = orig_getchar();
    io_args.call_count = 1;
    if (EOF != return_val) {
      io_args.bytes_read = 1;
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, getchar);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_getchar(), getchar);
  }
}

/*
char * wrapped_gets(char *s) {
  WRAPPER_ENTER(gets);

  // Get the handle for the original function
  gets_f orig_gets = (gets_f) gotcha_get_wrappee(io_fns[CURIOUS_GETS_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    curious_file_record_t *record = get_curious_file_record(STDIN_FILENO);
    if (NULL == record) {
      filesystem = NULL;
    } else {
      filesystem = record->filesystem;
    }

    curious_read_record_t io_args = {
      .bytes_read = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .function_id = CURIOUS_GETS_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    char *return_val = orig_gets(s);
    io_args.call_count = 1;
    if (NULL != return_val) {
      io_args.bytes_read = strlen(return_val);
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, gets);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_gets(s), gets);
  }
}
*/

//NOTE: not sure what to do for getc, since it could be a macro version of fgetc
//      or for ungetc, since it just pushes something onto the stream,
//      not to an actual file (or so it would seem)

size_t wrapped_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  WRAPPER_ENTER(fread);

  // Get the handle for the original function
  fread_f orig_fread = (fread_f) gotcha_get_wrappee(io_fns[CURIOUS_FREAD_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fileno(stream));
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_read_record_t io_args = {
      .bytes_read = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FREAD_ID,
    };
    curious_call_callbacks(CURIOUS_READ_CALLBACK, &io_args);

    // Call the original, saving the result...
    size_t return_val = orig_fread(ptr, size, nmemb, stream);
    io_args.call_count = 1;
    if (0 < return_val) {
      io_args.bytes_read = return_val * size;
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, fread);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_fread(ptr, size, nmemb, stream), fread);
  }
}

size_t wrapped_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  WRAPPER_ENTER(fwrite);

  // Get the handle for the original function
  fwrite_f orig_fwrite = (fwrite_f) gotcha_get_wrappee(io_fns[CURIOUS_FWRITE_ID].handle);

  // Only call callbacks the first time around
  if (1 == wrapper_call_depth && wrappers_enabled) {
    char *filesystem;
    char *mount_point;
    curious_file_record_t *record = get_curious_file_record(fileno(stream));
    if (NULL == record) {
      filesystem = NULL;
      mount_point = NULL;
    } else {
      filesystem = record->filesystem;
      mount_point = record->mount_point;
    }

    curious_write_record_t io_args = {
      .bytes_written = 0,
      .call_count = 0,
      .filesystem = filesystem,
      .mount_point = mount_point,
      .function_id = CURIOUS_FWRITE_ID,
    };
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK, &io_args);

    // Call the original, saving the result...
    size_t return_val = orig_fwrite(ptr, size, nmemb, stream);
    io_args.call_count = 1;
    if (0 < return_val) {
      io_args.bytes_written = return_val * size;
    }

    //...for the pot callbacks to use
    curious_call_callbacks(CURIOUS_WRITE_CALLBACK | CURIOUS_POST_CALLBACK, &io_args);

    WRAPPER_RETURN(return_val, fwrite);

    // and just call the original function every other time
  } else {
    WRAPPER_RETURN(orig_fwrite(ptr, size, nmemb, stream), fwrite);
  }
}

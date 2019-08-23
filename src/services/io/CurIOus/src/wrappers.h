#ifndef IO_WRAPPERS
  #define IO_WRAPPERS

  #include <sys/types.h>
  #include <sys/stat.h>
  #include <stdio.h>
  #include <gotcha/gotcha.h>
  #include <curious/curious.h>

  #ifdef __cplusplus
  extern "C" {
  #endif

  /*********
   * Types *
   *********/
  
  // This just keeps all the data about the original function in one place
  typedef struct io_function_data {
    curious_api_t api;
    gotcha_wrappee_handle_t handle;
  } io_function_data_t;

  void apply_wrappers(curious_apis_t apis);
  void disable_wrappers(void);

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

  // See io_arhs.h under include/curious for structs to serve as io_args 
  // to callbacks for corresponding functions
  
  /*********************
   * Wrapper Functions *
   *********************/
 
  // Actual declarations of wrappers

  // POSIX Wrappers:
  ssize_t wrapped_read(int fd, void *buf, size_t count);
  ssize_t wrapped_write(int fd, const void *buf, size_t count);
  int wrapped_open(const char *pathname, int flags, mode_t mode);
  int wrapped_close(int fd);
  int wrapped_stat(const char *path, struct stat *buf);
  int wrapped_xstat(int vers, const char *path, struct stat *buf);
  int wrapped_xstat64(int vers, const char *path, struct stat *buf);
  int wrapped_lstat(const char *path, struct stat *buf);
  int wrapped_lxstat(int vers, const char *path, struct stat *buf);
  int wrapped_lxstat64(int vers, const char *path, struct stat *buf);
  int wrapped_fstat(int fd, struct stat *buf);
  int wrapped_fxstat(int vers, int fd, struct stat *buf);
  int wrapped_fxstat64(int vers, int fd, struct stat *buf);
  
  // C stdio Wrappers:
  FILE * wrapped_fopen(const char *path, const char *mode);
  FILE * wrapped_fdopen(int fd, const char *mode);
  FILE * wrapped_freopen(const char *path, const char *mode, FILE *stream);
  int wrapped_fclose(FILE *fp);
  int wrapped_printf(const char *format, ...);
  int wrapped_fprintf(FILE *stream, const char *format, ...);
  int wrapped_vprintf(const char *format, va_list ap);
  int wrapped_vfprintf(FILE *stream, const char *format, va_list ap);
  /*
  int wrapped_scanf(const char *format, ...);
  int wrapped_fscanf(FILE *stream, const char *format, ...);
  int wrapped_vscanf(const char *format, va_list ap);
  int wrapped_vfscanf(FILE *stream, const char *format, va_list ap);
  */
  int wrapped_fgetc(FILE *stream);
  char * wrapped_fgets(char *s, int size, FILE *stream);
  int wrapped_getchar(void);
  //char * wrapped_gets(char *s);
  size_t wrapped_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
  size_t wrapped_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream);
  
  #ifdef __cplusplus
  } // extern "C"
  #endif
#endif

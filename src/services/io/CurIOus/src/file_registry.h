#ifndef FILE_REGISTRY
  #define FILE_REGISTRY

  #define STDIO_FILESYS "stdio"

  /*********
   * Types *
   *********/

  typedef struct file_record {
    char *path;
    char *filesystem;
    char *mount_point;
  } file_record_t;

  /**********************
   * Internal Functions *
   **********************/

  void init_file_registry();
  void finalize_file_registry();
  void destroy_file_record(file_record_t *record);
  // Should be called whenever a new file is opened, 
  // so that we can store information on that file
  int register_file_by_fd(int fd);
  int register_file(const char *path, int fd, char *filesystem, char *mount_point);
  int deregister_file(int fd);
  file_record_t * get_file_record(int fd);
#endif

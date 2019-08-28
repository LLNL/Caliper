#ifndef FILE_REGISTRY
  #define FILE_REGISTRY

  #define STDIO_FILESYS "stdio"

  /*********
   * Types *
   *********/

  typedef struct curious_file_record {
    char *path;
    char *filesystem;
    char *mount_point;
  } curious_file_record_t;

  /**********************
   * Internal Functions *
   **********************/

  void curious_init_file_registry();
  void curious_finalize_file_registry();
  void destroy_curious_file_record(curious_file_record_t *record);
  // Should be called whenever a new file is opened, 
  // so that we can store information on that file
  int curious_register_file_by_fd(int fd);
  int curious_register_file(const char *path, int fd, char *filesystem, char *mount_point);
  int curious_deregister_file(int fd);
  curious_file_record_t * get_curious_file_record(int fd);
#endif

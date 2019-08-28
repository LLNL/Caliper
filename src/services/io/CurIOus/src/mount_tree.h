#ifndef MOUNT_TREE
  #define MOUNT_TREE
  

void curious_init_mount_tree(void);
void curious_finalize_mount_tree(void);
void curious_get_filesystem(const char *full_path, char **filesystem, char **mount_point);

#endif

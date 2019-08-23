#ifndef MOUNT_TREE
  #define MOUNT_TREE
  
  #include "dynamic_array.h"

  #define ROOT_PATH "/"
  
  /*********
   * Types *
   *********/

  typedef struct mount_tree {
    char *name;
    char *full_path;
    char *filesystem;
    dynamic_array_t children;
  } mount_tree_t;

  /**********************
   * Internal Functions *
   **********************/

  void init_mount_tree(void);
  void finalize_mount_tree(void);
  void create_mount_tree(mount_tree_t *self, char *name, char *full_path, char *filesystem);
  void destroy_mount_tree(mount_tree_t *self);
  mount_tree_t * get_mount(char *full_path, char **name); 
  void add_mount(char *full_path, char *filesystem);
  void get_filesystem(const char *full_path, char **filesystem, char **mount_point);
  int compare_mount_tree_by_name(char *name, mount_tree_t *tree);
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "mount_tree.h"

#include "dynamic_array.h"

#define ROOT_PATH "/"

/*********
 * Types *
 *********/

typedef struct mount_tree {
  char *name;
  char *full_path;
  char *filesystem;
  curious_dynamic_array_t children;
} mount_tree_t;

/**********************
 * Internal Functions *
 **********************/
static void create_mount_tree(mount_tree_t *self, char *name, char *full_path, char *filesystem);
static void destroy_mount_tree(mount_tree_t *self);
static mount_tree_t * get_mount(char *full_path, char **name); 
static void add_mount(char *full_path, char *filesystem);
static int compare_mount_tree_by_name(char *name, mount_tree_t *tree);

static mount_tree_t root;

void curious_init_mount_tree(void) {
  create_mount_tree(&root, ROOT_PATH, ROOT_PATH, "");
  
  FILE *mount_file = fopen("/proc/mounts", "r");

  ssize_t bytes_read;
  char *line = NULL;
  size_t size = 0;
  #define VAL_SEP " "
  // Read every line in the list of mounts
  while (0 < (bytes_read = getline(&line, &size, mount_file))) {
    // Each line of /proc/mounts is a list of space-seperated values
    // We don't care about the first value, so just skip it
    strtok(line, VAL_SEP);
    
    // The second element is the full path to the mount
    char *full_path = strtok(NULL, VAL_SEP);

    // The third element is the type of filesystem the mount uses
    char *filesystem = strtok(NULL, VAL_SEP);

    add_mount(full_path, filesystem);
  }
  
  free(line);
  fclose(mount_file);
}

void curious_finalize_mount_tree(void) {
  destroy_mount_tree(&root);
}

void create_mount_tree(mount_tree_t *self, char *name, char *full_path, char *filesystem) {
  //printf("registering %s mount at %s\n", filesystem, full_path);
  
  self->name = strdup(name);
  self->full_path = strdup(full_path);
  self->filesystem = strdup(filesystem);
  create_curious_dynamic_array(&self->children, 0, sizeof(mount_tree_t));
}

void destroy_mount_tree(mount_tree_t *self) {
  free(self->name);
  free(self->full_path);
  free(self->filesystem);
  destroy_curious_dynamic_array(&self->children, (free_f) destroy_mount_tree);
}

mount_tree_t * get_mount(char *full_path, char **name) {
  if (0 == strcmp(full_path, ROOT_PATH)) {
    return &root;
  }

  mount_tree_t *cur_mount = &root;
  mount_tree_t *next_mount;
  #define DIR_SEP "/"
  // Look at every path elemet (i.e. each level of directories) in the full path...
  for (char *path_el = strtok(full_path, DIR_SEP); NULL != path_el; path_el = strtok(NULL, DIR_SEP)) {
    // The name should always be the last path element we've looked at
    if (NULL != name) {
      *name = path_el;
    }

    // ...see if there's a mount one level down which matches the current path element
    next_mount = find_in_curious_dynamic_array(&cur_mount->children,
                                       path_el,
                                       (compare_f) compare_mount_tree_by_name);
    // ...if we couldn't find a match, we've found the deepest relevant node,
    // meaning that we're done
    if (NULL == next_mount) {
      return cur_mount;

    // ...otherwise, we want to look at the childreen of the matching mount
    } else {
      cur_mount = next_mount;
    }
  }

  return &root;
}

void add_mount(char *full_path, char *filesystem) {
  // If the path in question is to the root, no need to go looking;
  // we should just set its filesystem now 
  if (0 == strcmp(full_path, ROOT_PATH)) {
    // make sure we don't overwrite that first setup
    // (the actual root entry is proper the first)
    if (0 != strcmp(root.filesystem, "")) {
      free(root.filesystem);
      root.filesystem = strdup(filesystem);
    }

    return;
  }
 
  char *name = NULL;
  char *path_copy = strdup(full_path);
  mount_tree_t *parent_mount = get_mount(path_copy, &name);

  if (NULL == parent_mount) {
    printf("No relevant mount found for %s\n", full_path);
  } else if (0 != strcmp(name, parent_mount->name)) {
    // Make a new child mount of the parent we found
    mount_tree_t mount;
    create_mount_tree(&mount, name, full_path, filesystem);
    append_to_curious_dynamic_array(&parent_mount->children, &mount);
  }
 
  // wait until after the mount tree nodehas been created to free this copy,
  // since name is part of that buffer
  free(path_copy);
}

void curious_get_filesystem(const char *path, char **filesystem, char **mount_point) {
  // get_mount uses strtok, which mangeles its input string,
  // so we only want to pass in a copy
  char *path_copy;
  
  // if the path starts from the root, it's absolute already, so just copy it 
  if ('/' == path[0]) {
    path_copy = strdup(path);
  
  // otherwise, we need to atually find the absolute path
  // and if we encounter an error, just set 
  } else {
    path_copy = realpath(path, NULL);

    // if we can't resolve the absolute path, we can't find the proper mount
    if (NULL == path_copy) {
      *filesystem = NULL;
      *mount_point = NULL;
      return;
    }
  }

  // Just find the deepest mount in the given path
  // and return which filesystem it uses 
  mount_tree_t *mount = get_mount(path_copy, NULL);

  // strdup and realpath both allocate a string, so we need to free it
  free(path_copy);

  *filesystem = mount->filesystem;
  *mount_point = mount->full_path;
}

int compare_mount_tree_by_name(char *name, mount_tree_t *tree) {
  return strcmp(name, tree->name);
}

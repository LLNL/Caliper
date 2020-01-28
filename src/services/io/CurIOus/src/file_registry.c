#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "file_registry.h"
#include "dynamic_array.h"
#include "mount_tree.h"

static curious_dynamic_array_t file_registry;

void curious_init_file_registry() {
  curious_init_mount_tree();
  create_curious_dynamic_array(&file_registry, 3, sizeof(curious_file_record_t));

  //lookup the actual filenames of the standard I/O files
  curious_register_file_by_fd(STDIN_FILENO);
  curious_register_file_by_fd(STDOUT_FILENO);
  curious_register_file_by_fd(STDERR_FILENO);
}

void curious_finalize_file_registry() {
  destroy_curious_dynamic_array(&file_registry, (free_f) destroy_curious_file_record);
  curious_finalize_mount_tree();
}

void destroy_curious_file_record(curious_file_record_t *record) {
  // We allocated space for the path string when we duplicated it
  free(record->path);
  
  record->path = NULL;
  record->filesystem = NULL;
  record->mount_point = NULL;
}

//TODO: add error states for improper fds
int curious_register_file_by_fd(int fd) {
  if (fd < 0) {
    return -1;
  }
  
  // Setup the basic I/O files
  // Assume we have at most a 6 digit file descriptor, since we can spare a few bytes
  #define DIGITS 6
  char fd_path[] = "/proc/self/fd/\0      ";
  size_t len = strlen(fd_path);

  // We make the assumption that the mount will be in the first 256 bytes,
  // since that's longer than any line in /proc/mounts
  #define BUF_SIZE 256
  char buf[BUF_SIZE];
  
  // append fd to fd_path
  int trunc_ret = snprintf(fd_path + len, DIGITS, "%d", fd);
  if(trunc_ret < 0 || trunc_ret >= DIGITS)
    fprintf(stderr, "Warning! err code: %i. fd (%i) may be truncated in fd_path (%s)\n",
	    trunc_ret, fd, fd_path);
  
  // see where the link stored at fd_path goes
  size_t bytes_written = readlink(fd_path, buf, BUF_SIZE);
  
  // readlink doesn't add a null terminator, so we should
  if (bytes_written < BUF_SIZE - 1) {
    buf[bytes_written] = '\0';
  } else {
    buf[BUF_SIZE - 1] = '\0';
  }

  // now register the file using that path
  char *filesystem;
  char *mount_point;
  curious_get_filesystem(buf, &filesystem, &mount_point);
  return curious_register_file(buf, fd, filesystem, mount_point);
}

int curious_register_file(const char *path, int fd, char *filesystem, char *mount_point) {
  curious_file_record_t *cur_record = (curious_file_record_t *) get_from_curious_dynamic_array(&file_registry, fd);

  // If the file is a pipe, then identify the pipe as the 'mount point'
  if (NULL == mount_point && 0 == strncmp(path, "pipe:", 5)) {
    mount_point = strdup(path);
  }

  //printf("Registering %s on %s filesystem as fd %d\n", path, filesystem, fd);

  // If the fd hasn't been used before...
  if (NULL == cur_record) {
    // We need to set up a new record
    curious_file_record_t new_record;
  
    // ...and intialise its fields
    new_record.path = strdup(path);
    new_record.filesystem = filesystem;
    new_record.mount_point = mount_point;

    //Ditto for a blank record to fill any extra space created
    //by adding this file record
    curious_file_record_t blank_record;
    blank_record.path = NULL;
    blank_record.filesystem = NULL;
    blank_record.mount_point = NULL;

    set_in_curious_dynamic_array(&file_registry, &new_record, &blank_record, fd);
  
  // ...otherwise...
  } else {
    // ...all we need to do is intialise the feilds, 
    // since we already have space for a record where we need it
    cur_record->path = strdup(path);
    cur_record->filesystem = filesystem;
    cur_record->mount_point = mount_point;
  }
  
  return 0;
}

int curious_deregister_file(int fd) {
  curious_file_record_t *cur_record = (curious_file_record_t *) get_from_curious_dynamic_array(&file_registry, fd);
 
  //printf("deregistering file %s on %s filesystem as %d\n", cur_record->path, cur_record->filesystem, fd);

  free(cur_record->path);
  cur_record->path = NULL;
  cur_record->filesystem = NULL;

  return 0;
}

// This is mostly here so we can confile dirct access to the file registry to this file
curious_file_record_t * get_curious_file_record(int fd) {
  return (curious_file_record_t *) get_from_curious_dynamic_array(&file_registry, fd); 
}

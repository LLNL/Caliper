#include <stdlib.h>
#include <string.h>

#include "dynamic_array.h"

void create_curious_dynamic_array(curious_dynamic_array_t *self, int len, size_t el_size) {
  //if the array is starting empty, then there isn't a first element;
  //setting it to -1 means we allways get out of bounds errors
  //and ensures that the next index is 0
  self->last_el = -1;

  self->len = len;
  self->el_size = el_size;
  self->array = malloc(len * el_size);
}

void destroy_curious_dynamic_array(curious_dynamic_array_t *self, free_f destroy_el) {
  if (NULL != destroy_el) {
    //destroy each element in the array...
    for (int i = 0; i <= self->last_el; ++i) {
      destroy_el(get_from_curious_dynamic_array(self, i));
    }
  }

  //...and only afterwards free the array itself
  free(self->array);
}

void append_to_curious_dynamic_array(curious_dynamic_array_t *self, void *new_el) {
  //if there's no more space left in the array...
  if (self->last_el + 1 >= self->len) {
    if (0 >= self->len) {
      //...either give it a single element or...
      self->len = 1;
    } else {
      //...double its length
      self->len *= 2;
    }

    self->array = realloc(self->array, self->len * self->el_size);
  }
  
  //either way, we can just copy the new element to space at 
  //the end of the array and update last_el accordingly
  self->last_el++;
  memcpy(get_from_curious_dynamic_array(self, self->last_el), new_el, self->el_size);
}

int remove_from_curious_dynamic_array(curious_dynamic_array_t *self, int index, int num_els, free_f destroy_el) {
  //TODO: make these actual errors
  if (index < 0 || index > self->last_el) {
    return -1;
  } else if (num_els < 0 || num_els > self->last_el - index) {
    return -2;
  } else if (0 == num_els) {
    //not an error, we just don't need to remove anything
    return 0;
  }

  // Free all the elements from the section to be removed
  if (NULL != destroy_el) {
    int end = index + num_els;
    for (int i = index; i < end; ++i) {
      destroy_el(get_from_curious_dynamic_array(self, i));
    }
  }

  int tail_count = self->last_el - num_els - index;

  // Move the remaining entries at the end of the array (if any)
  // forward to fill the vacated space
  if (tail_count > 0) {
    memmove(
      get_from_curious_dynamic_array(self, index),
      get_from_curious_dynamic_array(self, index + num_els),
      tail_count * self->el_size
    );
  }
  self->last_el -= num_els;
  
  return 0;
}

void set_in_curious_dynamic_array(curious_dynamic_array_t *self, void *new_el, void *filler_el, int index) {
  //if there's not enough space left in the array...
  if (index + 1 >= self->len) {
    while (index + 1 >= self->len) {
      if (0 >= self->len) {
        //...either give it a single element or...
        self->len = 1;
      } else {
        //...double its length
        self->len *= 2;
      }
    }

    //the loop above is mostly this way so we can do a single realloc no matter what
    self->array = realloc(self->array, self->len * self->el_size);
  }

  //use filler_el to fill in the space between the current end of the array
  //and the one we're adding - this allows the user to handle accesses to
  //this blank space, allowing us to keep the get interface simple
  while(index - self->last_el > 1) {
    append_to_curious_dynamic_array(self, filler_el);
  }
  
  //either way, we can just copy the new element to the indicated space 
  //in the array and update last_el accordingly
  if (index > self->last_el) {
    ++self->last_el;
  }
  memcpy(get_from_curious_dynamic_array(self, index), new_el, self->el_size);
}

void * get_from_curious_dynamic_array(curious_dynamic_array_t *self, int index) {
  if (index < 0 || index > self->last_el) {
    //just return NULL if the index is out of bounds
    return NULL;
  } else {
    return self->array + self->el_size * index;
  }
}
  
void * find_in_curious_dynamic_array(curious_dynamic_array_t *self, void *value, compare_f compare_to_el) {
  for (int i = 0; i <= self->last_el; ++i) {
    void *el = get_from_curious_dynamic_array(self, i);

    if (0 == compare_to_el(value, el)) {
      return el;
    }
  }

  return NULL;
}

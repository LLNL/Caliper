#ifndef curious_dynamic_array
  #define curious_dynamic_array

  /*********
   * Types *
   *********/

  typedef struct curious_dynamic_array {
    int last_el; //index of lat element in array
    int len; //number of elements in array
    size_t el_size; //size of each element in array
    void *array; //stores actual data
  } curious_dynamic_array_t;

  //a function with the same signature (and essential purpose) as free
  typedef void (* free_f)(void *);

  //a function which compares two values and returns 0 if they are equal
  //(ex: strcmp)
  typedef int (* compare_f)(void *value_0, void *value_1);

  /*******************************
   * Dynamic array API Functions *
   *******************************/

  void create_curious_dynamic_array(curious_dynamic_array_t *self, int len, size_t el_size);
  // NOTE: need to accept free function for each element, in case they have nested pointers
  void destroy_curious_dynamic_array(curious_dynamic_array_t *self, free_f destroy_el); 
  void append_to_curious_dynamic_array(curious_dynamic_array_t *self, void *new_el);
  int remove_from_curious_dynamic_array(curious_dynamic_array_t *self, int index, int num_els, free_f destroy_el);
  void set_in_curious_dynamic_array(curious_dynamic_array_t *self, void *new_el, void *filler_el, int index);
  void * get_from_curious_dynamic_array(curious_dynamic_array_t *self, int index);
  // Returns a pointer to the first element in the given array for which
  // compare_to_el(value <element from array>) returns 0, or NULL if no such element is found
  void * find_in_curious_dynamic_array(curious_dynamic_array_t *self, void *value, compare_f compare_to_el);
#endif

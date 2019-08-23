#ifndef CURIOUS
  #define CURIOUS

  #ifdef __cplusplus
  extern "C" {
  #endif

  #include <sys/types.h>

  /*********
   * Types *
   *********/

  // Error values
  typedef enum curious_err {
    CURIOUS_ERR_INVALID_CALLBACK_TYPE = 1,
  } curious_error_t;
 
  // Used to identify an CURIOUS instance
  typedef int curious_t;

  // Indicates what API an IO function originally came from 
  // (each should be a power of two, so that they can be properly or-ed together)
  typedef enum curious_api {
    CURIOUS_POSIX = 0x1,
    CURIOUS_CSTDIO = 0x2,
  } curious_api_t;

  // Just all the curious_api_ts bitwise or-ed together - used to indicate that every API should be wrapped
  #define CURIOUS_ALL_APIS (CURIOUS_POSIX | CURIOUS_CSTDIO)

  // Should be bitwise or of curious_api_ts - used to indicate which APIs to wrap functions from
  typedef int curious_apis_t;

  // Callback Attributes:

  #define CURIOUS_POST_CALLBACK 0x1 // use this if the callback is to be called after the original function,
                            // ommit it if it should be called before the original function
  // This comes before the function ids, despite the difficulty with indexing the bindings
  // because it allows us to add more functions AND avoid paddding the arrays with empty space
  // whil still maintaining the correspondence between ids and indices in both arrays
  
  // Also, if the callback is to be called after the original function, bitwise anding CURIOUS_POST_CALLBACK
  // with the curious_callback_type should return 1 (0 means it should be called before the original function)

  // Indicates what type of I/O operation the callback should be triggered by, and recieve data from
  typedef enum curious_callback_category {
    CURIOUS_READ_CALLBACK = 0x0,
    CURIOUS_WRITE_CALLBACK = 0x2,
    CURIOUS_METADATA_CALLBACK = 0x4,
  } curious_callback_category_t;

  // bitwise and this mask with an curious_callback_type_t to get the curious_callback_category
  #define CURIOUS_CALLBACK_CATEGORY_MASK 0x6

  // Bitwise or together callback attributes and IO function IDs to build an curious_callback_type
  // ex: CURIOUS_READ_CALLBACK = callback for before read
  //     CURIOUS_READ_CALLBACK | CURIOUS_POST_CALLBACK = callback for after read
  typedef int curious_callback_type_t;
  
  // The total number of valid curious_callback_types, mainly included for iteration purposes
  #define CURIOUS_CALLBACK_TYPE_COUNT 6 
  
  // IO function IDs 
  // Uniquely identify IO functions which can recieve callbacks;
  // specifically gives the function's index in the GOTCHA binding table
  typedef enum curious_function_id {
    #define CURIOUS_POSIX_START CURIOUS_READ_ID
    CURIOUS_READ_ID = 0,
    CURIOUS_WRITE_ID,
    CURIOUS_OPEN_ID,
    CURIOUS_CLOSE_ID,
    #define CURIOUS_STAT_START CURIOUS_STAT_ID
    CURIOUS_STAT_ID,
    CURIOUS__STAT_ID,
    CURIOUS_LSTAT_ID,
    CURIOUS__LSTAT_ID,
    CURIOUS_XSTAT_ID,
    CURIOUS__XSTAT_ID,
    CURIOUS_XSTAT64_ID,
    CURIOUS__XSTAT64_ID,
    CURIOUS_LXSTAT_ID,
    CURIOUS__LXSTAT_ID,
    CURIOUS_LXSTAT64_ID,
    CURIOUS__LXSTAT64_ID,
    CURIOUS_FSTAT_ID,
    CURIOUS__FSTAT_ID,
    CURIOUS_FXSTAT_ID,
    CURIOUS__FXSTAT_ID,
    CURIOUS_FXSTAT64_ID,
    CURIOUS__FXSTAT64_ID,
    #define CURIOUS_CSTDIO_START CURIOUS_FOPEN_ID
    CURIOUS_FOPEN_ID,
    CURIOUS_FOPEN64_ID,
    CURIOUS_FDOPEN_ID,
    CURIOUS_FREOPEN_ID,
    CURIOUS_FCLOSE_ID,
    CURIOUS_PRINTF_ID,
    CURIOUS_FPRINTF_ID,
    CURIOUS_VPRINTF_ID,
    CURIOUS_VFPRINTF_ID,
    /*
    CURIOUS_SCANF_ID,
    CURIOUS_FSCANF_ID,
    CURIOUS_VSCANF_ID,
    CURIOUS_VFSCANF_ID,
    */
    CURIOUS_FGETC_ID,
    CURIOUS_FGETS_ID,
    CURIOUS_GETCHAR_ID,
    //CURIOUS_GETS_ID,
    CURIOUS_FREAD_ID,
    CURIOUS_FWRITE_ID,
  } curious_function_id_t;

  // This is just to help with iteration
  #define CURIOUS_FUNCTION_COUNT 36

  // I/O Record Types:

  // For callbacks to read functions 
  // (i.e. ones registered with a curious_callback_type containing CURIOUS_READ_CALLBACK)
  typedef struct curious_read_record {
    size_t bytes_read;
    unsigned int call_count;
    char *filesystem;
    char *mount_point;
    curious_function_id_t function_id;
  } curious_read_record_t;
  
  // For callbacks to write functions 
  // (i.e. ones registered with a curious_callback_type containing CURIOUS_WRITE_CALLBACK)
  typedef struct curious_write_record {
    size_t bytes_written;
    unsigned int call_count;
    char *filesystem;
    char *mount_point;
    curious_function_id_t function_id;
  } curious_write_record_t;

  // For callbacks to metadata functions 
  // (i.e. ones registered with a curious_callback_type containing CURIOUS_METADATA_CALLBACK)
  typedef struct curious_metadata_record {
    unsigned int call_count;
    char *filesystem;
    char *mount_point;
    curious_function_id_t function_id;
  } curious_metadata_record_t;

  // All IO callbacks must have this basic signature,
  // which allows all neccessary information to be cast as needed
  // from the two arguments (usr_args being defined by the person
  // using this API, io_record being defined by this API to pass in
  // appropriate data for the function type)
  typedef void (*curious_callback_f)(curious_callback_type_t type, void *usr_args, void *io_record);
  
  /*****************
   * API Functions *
   *****************/

  // Prepares library for use; call at the start of a program which uses CURIOUS
  // Use curious_apis to indicate which API's functions to wrap, by bitwise or-ing together
  // the corresponding curious_api_t constants, or pass in ALL_APIS to use them all
  // Returns a unique identifier for this application's use of CURIOUS
  curious_t curious_init(curious_apis_t curious_apis);

  // Cleans up all library variables; call at the end of a program which uses CURIOUS
  void curious_finalize(curious_t curious_inst);

  // Adds the given callback to the list of those called before or after a wrapped function,
  // as indicated by type. Callback will be passed the given usr_args as its first argument
  // when called and a struct containing all of the arguments to the wrapping function
  // in a function-dependent struct as the second argument (see io_record.h)
  int curious_register_callback(
    curious_t curious_inst,
    curious_callback_type_t type,
    curious_callback_f callback,
    void *usr_args
  );

  #ifdef __cplusplus
  } // extern "C"
  #endif
#endif

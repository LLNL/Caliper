Annotation API
================================

Caliper provides source-code instrumentation APIs for C and C++ to
create and access Caliper context attributes. 

High-level annotation macros
--------------------------------

For most source-code markup use cases, we recommend using Caliper's
annotation macros. They provide the easiest way to annotate functions,
loops, statements, and user-defined source-code regions.

.. c:macro:: CALI_MARK_FUNCTION_BEGIN
.. c:macro:: CALI_MARK_FUNCTION_END
             
   Mark begin and end of a function. The annotations must be placed at
   the top and on *all* function exit points, respectively. Will
   export the annotated functions by name in the pre-defined `function`
   attribute.

   For C++ codes, we recommend using :c:macro:`CALI_CXX_MARK_FUNCTION`
   instead. 

.. c:macro:: CALI_CXX_MARK_FUNCTION

   Mark begin and end of a function. Should be placed at the top of the
   function, and will automatically "close" the function at any return
   point. Will export the annotated function by name in the pre-defined
   `function` attribute. Only available in C++.

.. c:function:: CALI_MARK_LOOP_BEGIN(loop_id, name)
.. c:function:: CALI_MARK_LOOP_END(loop_id)                

   Mark begin and end of a loop. Will export the user-provided loop name
   in the pre-defined `loop` attribute.

   The macros should be placed before and after the loop of interest.
   Users must ensure proper begin/end matching: If the surrounding
   function can be exited from within the loop (e.g., from a `return`
   statement within the loop), an exit marker must be placed there
   as well.

   :param loop_id: Alphanumerical identifier. Must be unique within the
                   surrounding scope.
   :param name: The loop name which will be exported to the `loop`
                attribute. This macro argument must be convertible
                into a `const char*`.

.. c:function:: CALI_MARK_ITERATION_BEGIN(loop_id, iteration)
.. c:function:: CALI_MARK_ITERATION_END(loop_id)

   Mark begin and end of a loop iteration. The loop must be
   annotated with the loop annotation macros
   (:c:func:`CALI_MARK_LOOP_BEGIN()` / :c:func:`CALI_MARK_LOOP_END()`).

   The macros should be placed at the top and bottom of the loop body.
   If an iteration can be left prematurely (e.g., from a `continue`, 
   `break`, or `return` statement), an end marker must be placed there
   as well.

   The iteration number will be exported in the attribute `iteration#name`,
   where `name` is the loop name given to :c:func:`CALI_MARK_LOOP_BEGIN()`.

   :param loop_id: Loop identifier, must match the identifier given to
                   :c:func:`CALI_MARK_LOOP_BEGIN()`.
   :param iteration: Current iteration number. This macro argument must be
                     convertible to `int`.

.. c:function:: CALI_MARK_BEGIN(name)
.. c:function:: CALI_MARK_END(name)

   Mark begin and end of a user-defined code region. The macros should be
   placed around the code region of interest. The user-provided region name
   will be exported in the pre-defined `annotation` attribute.

   Users must ensure proper nesting: Each region `begin` must be
   matched by a corresponding `end` in the correct order.  Regions may
   be nested within another, but they cannot overlap partially.

   :param name: The user-defined region name. Must be convertible into
                a `const char*`.

.. c:function:: CALI_WRAP_STATEMENT(name, statement)

   "Wraps" Caliper annotations around a C/C++ statement, or group of statments.
   The user-provided name will be exported to the `statement` attribute.

   :param name: The user-defined region name. Must be convertible into
                a `const char*`.
   :param statement: C/C++ statement(s) that should be wrapped. The
                     statements must complete within the wrapped
                     region, that is, they cannot branch out of the
                     macro (e.g.  with `goto`, `continue`, `break`, or
                     `return`).

Example: High-level annotation macros in C
............................................

.. code-block:: c

   #include <caliper/cali.h>
   #include <stdlib.h>

   int foo(int count)
   {
     CALI_MARK_FUNCTION_BEGIN;

     if (count == 0) {
       CALI_MARK_FUNCTION_END; /* Must mark _all_ function exit points! */
       return;
     }
                
     CALI_MARK_LOOP_BEGIN(fooloop, "example.fooloop");

     for (int i = 0; i < count; ++i) {
       CALI_MARK_ITERATION_BEGIN(fooloop, i);

       CALI_MARK_ITERATION_END(fooloop);
     }

     CALI_MARK_LOOP_END(fooloop);
                
     CALI_MARK_FUNCTION_END;
   }

   int main(int argc, char* argv[])
   {
     CALI_MARK_FUNCTION_BEGIN;

     CALI_WRAP_STATEMENT( "example.init",
       int count = argc > 1 ? atoi(argv[1]) : 4 );

     foo(count);

     CALI_MARK_BEGIN("output");
     printf("Done.\n");
     CALI_MARK_END("output");

     CALI_MARK_FUNCTION_END;
   }

In an event trace configuration, this program produces the following snapshot
records: ::

  function statement      loop              annotation iteration#example.fooloop 
  main     
  main     example.init 
  main                    
  main/foo                
  main/foo                example.fooloop 
  main/foo                example.fooloop                                      0 
  main/foo                example.fooloop                             
  main/foo                example.fooloop                                      1 
  main/foo                example.fooloop                             
  main/foo                example.fooloop                                      2 
  main/foo                example.fooloop                             
  main/foo                example.fooloop                                      3 
  main/foo                example.fooloop                             
  main/foo
  main
  main                                    output
  main                                                                  

Example: High-level annotation macros in C++
............................................

A C++ example producing the same output as above:

.. code-block:: c++

   #include <caliper/cali.h>
   #include <cstdlib>

   int foo(int count)
   {
     CALI_CXX_MARK_FUNCTION;

     if (count == 0)
       return;
                
     CALI_CXX_MARK_LOOP_BEGIN(fooloop, "example.fooloop");

     for (int i = 0; i < count; ++i) {
       CALI_CXX_MARK_LOOP_ITERATION(fooloop, i);

       // ... do work ...
     }

     CALI_CXX_MARK_LOOP_END(fooloop);
   }

   int main(int argc, char* argv[])
   {
     CALI_CXX_MARK_FUNCTION;

     CALI_WRAP_STATEMENT( "example.init",
       int count = argc > 1 ? atoi(argv[1]) : 4 );

     foo(count);

     CALI_MARK_BEGIN("output");
     std::cout << "Done." << std::endl;
     CALI_MARK_END("output");
   }
                

C++ annotation API
--------------------------------

The `cali::Annotation` class provides the C++ instrumentation interface.

.. cpp:class:: cali::Annotation

   #include <caliper/Annotation.h>

   Instrumentation interface to add and manipulate context attributes

   The Annotation class is the primary source-code instrumentation interface
   for Caliper. Annotation objects provide access to named Caliper context 
   attributes. If the referenced attribute key does not exist yet, it will be 
   created automatically.

   Example:

   .. code-block:: c++

     cali::Annotation phase_ann("myprogram.phase");
     
     phase_ann.begin("Initialization");
       // ...
     phase_ann.end();

   This example creates an annotation object for attributes with the
   ``myprogram.phase`` key, and uses the :cpp:func:`cali::Annotation::begin` and
   :cpp:func:`cali::Annotation::end` methods to mark a section of code where
   that attribute is set to "Initialization".

   Note that the access to attributes through Annotation 
   objects is not exclusive: two different Annotation objects can reference and
   update the same context attribute.

   .. cpp:function:: Annotation(const char* name, \
        int properties = 0)

      Constructor. Constructs an annotation object to manipulate
      attributes. If no attribute key with the given name exists yet,
      it will be created on the first invocation of a ``begin`` or
      ``set`` call.

      :param const char* name: Attribute name. 
      :param int properties: Properties given to the attribute when it \
         is created. A combination of :c:type:`cali_attr_properties` flags \
         combined using a bitwise ``or``. 

   .. cpp:function:: Annotation& begin(int value)
                     Annotation& begin(double value)
                     Annotation& begin(const char* value)

      Add new value for the referenced attribute to the blackboard.
      If there is already a value for the referenced attribute on the
      blackboard, the new value will be stacked on top.

      Overloaded variants are provided for integer, floating point,
      and string values. The value must match the type of the
      referenced attribute key; e.g., string values can only be
      assigned to attributes of type :c:macro:`CALI_TYPE_STRING`.  The type
      of a not-yet-existing attribute is defined by the first call to
      ``begin`` or ``set``.

      :return: Reference to the Annotation object, which can be \
               used to build a `Guard` scope guard object.

   .. cpp:function:: Annotation& begin()

      This 'value-less' variant can be used for marking named code
      regions without having to set a specific value. Internally, this
      will create boolean-type attribute and set it to ``true``.

   .. cpp:function:: Annotation& begin(cali_attr_type type, \
        void* ptr, uint64_t size)

      Generic version.

      :param cali_attr_type type: Value datatype
      :param void* ptr: Address of value
      :param uint64_t size: Object size

   .. cpp:function:: Annotation& set(int value)
                     Annotation& set(double value)
                     Annotation& set(const char* value)
                     Annotation& set(cali_attr_type type, void* ptr, uint64_t size)

      Set value for the referenced attribute on the blackboard.

      Works like :cpp:func:`cali::Annotation::begin`, except instead
      of stacking a new value on top of an existing one, ``set``
      overwrites the existing value.

   .. cpp:function:: void end()

      Remove top-most value of the referenced attribute from the blackboard.

      
C and Fortran annotation API
--------------------------------

Like the C++ :cpp:class:`Annotation` class, the C/Fortran API provides
``begin/set/end`` functions to add, overwrite, and remove attribute
values from the blackboard.

The Fortran API is a thin wrapper around the C API. Fortran subroutine
names and semantics are identical to the respective C versions.

.. c:function:: cali_id_t cali_create_attribute(const char* name, \
     cali_attr_type type, int properties)

   Create an attribute key using the given name, type, and properties,
   and return its ID. If an attribute with the given name already
   exists, return its ID instead.

   :param const char* name: Attribute name
   :param cali_attr_type type: Attribute type
   :param int properties: Attribute properties. A combination of \
      :c:type:`cali_attr_properties` flags combined using a bitwise ``or``.
   :return: Attribute ID of the newly created attribute, \
      or already existing attribute with the given name.

   Fortran signature: ::

     subroutine cali_create_attribute(name, type, properties, id)
       character(len=*),        intent(in)  :: name
       integer,                 intent(in)  :: type
       integer,                 intent(in)  :: properties
       integer(kind=C_INT64_T), intent(out) :: id

.. c:function:: cali_err cali_begin_double(cali_id_t attr, double val)
                cali_err cali_begin_int(cali_id_t attr, int val)
                cali_err cali_begin_string(cali_id_t attr, const char* val)

   Add new value for attribute with the given ID to the blackboard.
   If there is already a value for the referenced attribute on the
   blackboard, the new value will be stacked on top.

   Variants are provided for integer, floating point,
   and string values. The value must match the type of the
   referenced attribute key; e.g., string values can only be
   assigned to attributes with type ``CALI_TYPE_STRING``.

   :param cali_id_t attr: Attribute ID
   :param val: Value
   :return: Error flag. ``CALI_SUCCESS`` if no error.

   Fortran signatures: ::

       subroutine cali_begin_string(id, val, err)
         integer(kind=C_INT64_T),     intent(in) :: id
         character(len=*),            intent(in) :: val
         integer(kind(CALI_SUCCESS)), intent(out), optional :: err

       subroutine cali_begin_int(id, val, err)
         integer(kind=C_INT64_T),     intent(in) :: id
         integer,                     intent(in) :: val
         integer(kind(CALI_SUCCESS)), intent(out), optional :: err

       subroutine cali_begin_double(id, val, err)
         integer(kind=C_INT64_T),     intent(in) :: id
         real*8,                      intent(in) :: val
         integer(kind(CALI_SUCCESS)), intent(out), optional :: err

.. c:function:: cali_err cali_begin(cali_id_t attr)

   This 'value-less' variant can be used for marking named code
   regions without having to set a specific value. Internally, this
   will set a boolean-type attribute to ``true``.

   Fortran signature: ::

       subroutine cali_begin(id, err)
         integer(kind=C_INT64_T),     intent(in) :: id
         integer(kind(CALI_SUCCESS)), intent(out), optional :: err

.. c:function:: cali_err cali_set_double(cali_id_t attr, double val) 
                cali_err cali_set_int(cali_id_t attr, int val)
                cali_err cali_set_string(cali_id_t attr, const char* val)

   Set value for the referenced attribute on the blackboard.

   These functions work like their corresponding ``begin`` variants,
   except instead of stacking a new value on top of an existing one,
   ``set`` overwrites the existing value.

   :param cali_id_t attr: Attribute ID
   :param val: Value
   :return: Error flag. ``CALI_SUCCESS`` if no error.

   Fortran signatures: ::

       subroutine cali_set_string(id, val, err)
         integer(kind=C_INT64_T),     intent(in) :: id
         character(len=*),            intent(in) :: val
         integer(kind(CALI_SUCCESS)), intent(out), optional :: err

       subroutine cali_set_int(id, val, err)
         integer(kind=C_INT64_T),     intent(in) :: id
         integer,                     intent(in) :: val
         integer(kind(CALI_SUCCESS)), intent(out), optional :: err

       subroutine cali_set_double(id, val, err)
         integer(kind=C_INT64_T),     intent(in) :: id
         real*8,                      intent(in) :: val
         integer(kind(CALI_SUCCESS)), intent(out), optional :: err

.. c:function:: cali_err cali_set(cali_id_t attr, \
     const void* ptr, size_t size)

   Generic version. This function allows setting values for attributes
   with data types other than integer, string, or double; in
   particular user-defined raw data (`CALI_TYPE_USR`).
   
   :param cali_id_t attr: Attribute ID
   :param void* ptr: Address of value
   :param size_t size: Size of value in bytes.
   :return: Error flag; ``CALI_SUCCESS`` if no error.
            
   This function is not yet implemented in Fortran. 

.. c:function:: cali_err cali_end(cali_id_t attr)

   Remove top-most value of the referenced attribute from the blackboard.

   Fortran signature: ::

     subroutine cali_end(id, err)
       integer(kind=C_INT64_T),     intent(in) :: id
       integer(kind(CALI_SUCCESS)), intent(out), optional :: err

.. c:function:: cali_err cali_begin_double_byname(const char* attr_name, double val)
                cali_err cali_begin_int_byname(const char* attr_name, int val)
                cali_err cali_begin_string_byname(const char* attr_name, const char* val)
                cali_err cali_begin_byname(const char* attr_name)
                cali_err cali_set_double_byname(const char* attr_name, double val)
                cali_err cali_set_int_byname(const char* attr_name int val)
                cali_err cali_set_string_byname(const char* attr_name, const char* val)
                cali_err cali_end_byname(const char* attr_name)

   The ``_byname`` convenience functions reference attributes directly
   through their name. If no attribute key with the given name exists
   yet, it will be created. Thus, the following examples produce the
   same result:

   .. code-block:: c

      cali_id_t attr = cali_create_attribute("my.attribute",
        CALI_TYPE_INT, CALI_ATTR_DEFAULT);

      cali_set_int(attr, 42);

   .. code-block:: c

      cali_set_int_byname("my.attribute", 42);

   As the ``_byname`` variants do perform an extra string lookup, it
   is better to use the by-ID lookup variants for performance-critical
   sections.

   :param const char* attr_name: Attribute name
   :param val: Value
   :return: Error flag. ``CALI_SUCCESS`` if no error.


   Fortran signatures: ::

     subroutine cali_begin_string_byname
       character(len=*), intent(in) :: attr_name
       character(len=*), intent(in) :: val
       integer(kind(CALI_SUCCESS)), intent(out), optional :: err
       
     subroutine cali_begin_int_byname
       character(len=*), intent(in) :: attr_name
       integer,          intent(in) :: val
       integer(kind(CALI_SUCCESS)), intent(out), optional :: err

     subroutine cali_begin_double_byname
       character(len=*), intent(in) :: attr_name
       real*8,           intent(in) :: val
       integer(kind(CALI_SUCCESS)), intent(out), optional :: err

     subroutine cali_set_string_byname
       character(len=*), intent(in) :: attr_name
       character(len=*), intent(in) :: val
       integer(kind(CALI_SUCCESS)), intent(out), optional :: err
       
     subroutine cali_set_int_byname
       character(len=*), intent(in) :: attr_name
       integer,          intent(in) :: val
       integer(kind(CALI_SUCCESS)), intent(out), optional :: err

     subroutine cali_set_double_byname
       character(len=*), intent(in) :: attr_name
       real*8,           intent(in) :: val
       integer(kind(CALI_SUCCESS)), intent(out), optional :: err

     subroutine cali_end_byname
       character(len=*), intent(in) :: attr_name
       integer(kind(CALI_SUCCESS)), intent(out), optional :: err

Examples
................................

The following examples demonstrate basic Caliper source-code
annotations in C and Fortran. Similar to the C++ example in
:doc:`usage`, they mark *initialization* and *loop* phases in a
program, and export the main loop's iteration counter in the
*iteration* attribute.

C:

.. code-block:: c

   #include <caliper/cali.h>
   
   int main() {
     /* Mark "initialization" phase */
     cali_begin_byname("initialization");
     int count = 4;
     cali_end_byname("initialization");

     if (count > 0) {
       /* Mark "loop" phase */
       cali_begin_byname("loop");

       /* Create iteration counter attribute with CALI_ATTR_ASVALUE property */ 
       cali_id_t iter_attr =
         cali_create_attribute("iteration", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

       for (int i = 0; i < count; ++i) {
         /* Update iteration counter attribute */
         cali_set_int(iter_attr, loop);

         /* A Caliper snapshot taken at this point will contain
          * { "loop", "iteration"=<i> }
          */

         /* perform computation */
       }

       /* Clear the iteration counter attribute (otherwise, snapshots taken
        * after the loop will still contain the last iteration value)
        */
       cali_end(iter_attr);
       
       /* End "loop" phase */
       cali_end_byname("loop");
     }
   }

Fortran: ::

  program testf03
    use Caliper

    implicit none

    integer                    :: cali_ret
    integer(kind(CALI_INV_ID)) :: iter_attr
    integer                    :: i, count

    ! Mark "initialization" phase
    call cali_begin_byname('initialization')
    count = 4
    call cali_end_byname('initialization')

    if (count .gt. 0) then
       ! Mark "loop" phase
       call cali_begin_byname('loop')

       ! create attribute for iteration counter with CALI_ATTR_ASVALUE property
       call cali_create_attribute('iteration', CALI_TYPE_INT, &
            CALI_ATTR_ASVALUE, iter_attr)

       do i = 1,count
          ! Update iteration counter attribute
          call cali_set_int(iter_attr, i)

          ! A Caliper snapshot taken at this point will contain
          ! { "loop", "iteration"=<i> } 

          ! perform calculation
       end do

       ! Clear the iteration counter attribute (otherwise, snapshots taken
       ! after the loop will still contain the last iteration value)
       call cali_end(iter_attr, cali_ret)

       ! Checking return value (not required, but good style)
       if (cali_ret .ne. CALI_SUCCESS) then
          print *, "cali_end returned with", cali_ret
       end if

       ! End "loop" phase
       call cali_end_byname('loop')
    end if
  end program testf03

  
Constants and types
--------------------------------

.. c:macro:: CALI_INV_ID

   Indicates an invalid (attribute) ID value.

.. c:type:: cali_attr_type

   Defines the datatype of attributes.

   .. c:macro:: CALI_TYPE_INV
      Indicates an undefined type
   .. c:macro:: CALI_TYPE_USR
      A user-defined type.
   .. c:macro:: CALI_TYPE_INT 
                CALI_TYPE_UINT 
                CALI_TYPE_STRING 
                CALI_TYPE_ADDR 
                CALI_TYPE_DOUBLE 
                CALI_TYPE_BOOL 
                CALI_TYPE_TYPE
      
.. c:type:: cali_attr_properties

   Attribute property flags. Attribute properties can be defined by
   combining the following property flags using bitwise ``or``:

   .. c:macro:: CALI_ATTR_DEFAULT
                
      Default attribute flags. Selects :c:macro:`CALI_ATTR_SCOPE_THREAD`
      
   .. c:macro:: CALI_ATTR_ASVALUE

      By default, all attributes are stored in the generalized context
      tree. Attributes with this flag will instead be stored
      explicitly as ``key:value`` pairs on the blackboard and in
      snapshot records.

   .. c:macro:: CALI_ATTR_NOMERGE

      By default, attributes are merged into a single generalized
      context tree. Attributes with this flag will be placed in a tree
      of their own.

   .. c:macro:: CALI_ATTR_SCOPE_PROCESS
                CALI_ATTR_SCOPE_THREAD
                CALI_ATTR_SCOPE_TASK

      Define the scope of the attribute. These flags are mutually
      exclusive.

   .. c:macro:: CALI_ATTR_SKIP_EVENTS

      Skip executing Caliper's event callback functions when updating
      this attribute.

   .. c:macro:: CALI_ATTR_HIDDEN

      Do not export this attribute into snapshot records.

.. c:type:: cali_err

   Error flag.

   .. c:macro:: CALI_SUCCESS

      Successful execution; no error.

   .. c:macro:: CALI_EINV

      Invalid function input parameter, e.g. providing an attribute ID
      that does not exist.

   .. c:macro:: CALI_ETYPE

      Type mismatch, e.g. trying to assign an integer value to a
      :c:macro:`CALI_TYPE_STRING` attribute.

         
C control API
--------------------------------


.. c:function:: void cali_push_snapshot(int scope, int n, \
                       const cali_id_t trigger_info_attr_list[], \
                       const void*     trigger_info_val_list[], \
                       const size_t    trigger_info_size_list[]);

   This function triggers a snapshot and puts it into Caliper's processing
   queue. Optionally, users can provide a list of trigger (event) info
   parameters to include in the snapshot.

   :param scope: Indicates which scopes (process, thread, or task) the
                 snapshot should span.
   :param n:     Number of optional trigger info entries.
   :param trigger_info_attr_list: Array of attribute IDs for the
                                  trigger info entries.
   :param trigger_info_val_list:  Array of pointers to values for the
                                  trigger info entries.
   :param trigger_info_size_list: Array to sizes of the provided
                                  trigger info values. 

   The given trigger info (or event) attributes will be attached to
   the generated snapshot, but not retained on the blackboard.

   Example:

.. code-block:: c

   // ...

   cali_begin_byname("push_snapshot_example");

   cali_id_t int_attr =
     cali_create_attribute("snapshot.intarg", CALI_TYPE_INT,
       CALI_ATTR_DEFAULT);
   cali_id_t str_attr =
     cali_create_attribute("snapshot.strarg", CALI_TYPE_STRING,
       CALI_ATTR_DEFAULT);

   const char* strval = "MySnapshot";
   int64_t     intval = 42;

   cali_id_t trigger_attr[2] = { int_attr, str_attr };
   void*     trigger_vals[2] = { &intval,  strval   };
   size_t    trigger_size[2] = { sizeof(int64_t),
                                 strlen(strval) };

   /* Trigger snapshot with process and thread-local
      blackboard contents */
   
   cali_push_snapshot(CALI_SCOPE_THREAD | CALI_SCOPE_PROCESS,
     2, /* number of trigger info attributes */,
     trigger_attr, trigger_val, trigger_size);
     
   cali_end_byname("push_snapshot_example");

   // ...

This example program will generate a snapshot with (at least) the
following contents: ::

  push_snapshot_example=true,snapshot.intarg=42,snapshot.strarg=MySnapshot


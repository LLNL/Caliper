Annotation API
================================

Caliper provides source-code instrumentation APIs for C and C++ to
create and access Caliper context attributes. 

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
   
C++ annotation API
--------------------------------

The `cali::Annotation` class provides the C++ instrumentation interface.

.. cpp:class:: cali::Annotation

   #include <Annotation.h>

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
values from the blackboard. The Fortran API is a thin wrapper around
the C API. Fortran subroutine names and semantics are identical to the
respective C versions.

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

   Generic version.

   :param cali_id_t attr: Attribute ID
   :param void* ptr: Address of value
   :param uint64_t size: Size of value in bytes.
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

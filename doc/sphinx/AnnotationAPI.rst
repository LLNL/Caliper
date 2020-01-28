Source-code Annotations
================================

Caliper provides source-code annotation APIs to mark and name code
regions or features of interest. The high-level annotation API
provides easy access to pre-defined annotation types. The low-level
annotation API lets users create their own context attributes to
capture custom code features.


High-level Annotations
--------------------------------

Caliper provides pre-defined annotation types for functions, loops,
and user-defined code regions, and a set of C and C++ macros to mark
and name those entities.

Function Annotations
................................

Function annotations export the name of the annotated function, which
is taken from the ``__FUNC__`` compiler macro. The annotations should
be placed within a function of interest, ideally near the top. In C,
the function begin and *all* exit points must be marked. In C++, only
the begin has to be marked. Caliper exports the hierarchy of annotated
functions in the `function` attribute.

C++ example:

.. code-block:: c++

   #include <caliper/cali.h>

   int foo(int r)
   {
     CALI_CXX_MARK_FUNCTION; // Exports "function=foo"

     if (r > 0)
       return r-1;

     return r;
     // In C++, the function annotation is closed automatically
   }

C example:

.. code-block:: c

   #include <caliper/cali.h>

   int foo(int r)
   {
     CALI_MARK_FUNCTION;       /* Exports "function=foo"          */

     if (r > 0) {
       CALI_MARK_FUNCTION_END; /* ALL exit points must be marked! */
       return r-1;
     }

     CALI_MARK_FUNCTION_END;   /* ALL exit points must be marked! */
     return r;
   }

Loop Annotations
................................

Loop annotations mark and name loops. The annotations should be placed
immediately outside a loop of interest. Both begin and end of the loop
have to be marked. Optionally, the loop iterations themselves can be
marked and the iteration number exported by placing loop iteration
annotations inside the loop body.  Caliper exports the loop name in
the `loop` attribute. Loop iterations are exported in per-loop
attributes named `iteration#name`, where `name` is the user-provided
loop name given to the loop annotation.

C++ example:

.. code-block:: c++

   #include <caliper/cali.h>

   CALI_CXX_MARK_LOOP_BEGIN(myloop_id, "myloop"); // exports loop=myloop
   for (int i = 0; i < ITER; ++i) {
     CALI_CXX_MARK_LOOP_ITERATION(myloop_id, i);  // exports iteration#myloop=<i>

     // In C++, the iteration annotation is closed automatically
   }
   CALI_CXX_MARK_LOOP_END(myloop_id);

C example:

.. code-block:: c++

   #include <caliper/cali.h>

   CALI_MARK_LOOP_BEGIN(myloop_id, "myloop"); // exports loop=myloop
   for (int i = 0; i < ITER; ++i) {
     CALI_MARK_ITERATION_BEGIN(myloop_id, i); // exports iteration#myloop=<i>

     if (test(i) == 0) {
       CALI_MARK_ITERATION_END(myloop_id);    // must mark ALL iteration exit points
       break;
     }

     CALI_MARK_ITERATION_END(myloop_id);      // must mark ALL iteration exit points
   }
   CALI_MARK_LOOP_END(myloop_id);

Code-region Annotations
................................

Code-region annotations mark and name user-defined source code
regions. Caliper exports the region names in the `annotation`
attribute. Annotated code regions must be properly nested (see
`Nesting`_).

Example:

.. code-block:: c++

   #include <caliper/cali.h>

   CALI_MARK_BEGIN("Important code"); // exports annotation="Important code"
   // ... important code
   CALI_MARK_END("Important code");

Nesting
................................

Annotated source-code regions of any of the pre-defined context
attributes (`function`, `loop`, and `annotation`) can be nested within
each other. Caliper preserves this nesting information. Users must
ensure that the nesting is correct. That is, annotated code regions
have to be enclosed completely within each other; they cannot
partially overlap. Example:

.. code-block:: c++

   #include <caliper/cali.h>

   int foo()
   {
     CALI_CXX_MARK_FUNCTION;

     CALI_MARK_BEGIN("outer");
     CALI_MARK_BEGIN("inner"); // The hierarchy is now "foo/outer/inner"

   #if 0
     CALI_MARK_END("outer");   // Error! Must end "inner" before "outer"!
     CALI_MARK_END("inner");
   #endif

     CALI_MARK_END("inner");
     CALI_MARK_END("outer");   // Correct nesting
   }

To annotate arbitrary features or regions that can overlap with
others, use the low-level annotation API and create a user-defined
context attribute.

Low-level Annotation API
--------------------------------

The "low-level" annotation API lets users create and export additional
context attributes with user-defined names to capture custom code
features or annotate arbitrary, non-nested code regions.

Attribute keys
................................

Context attributes are the basic element in Caliper's key:value data
model. The high-level annotation API uses pre-defined attribute keys,
but users can create their own. Attribute keys have a unique name, and
store the attribute's data type as well as optional property flags.
Property flags control how the Caliper runtime system handles the
associated attributes.

.. doxygenenum:: cali_attr_properties
   :project: caliper

Attribute keys can be created with :c:func:`cali_create_attribute()`:

.. doxygenfunction:: cali_create_attribute
   :project: caliper

C annotation API
................................

The C and Fortran annotation API provides the
``cali_begin_*[byname]``, ``cali_end[_byname]``, and
``cali_set_*_[byname]`` family of functions. Their behavior is as
follows:

``begin``
  Marks the begin of a region with the given attribute/value. The new
  value will be nested under already open regions of the same
  attribute.

``set``
  Sets or overwrites the top-most value for the given attribute.

``end``
  Closes the top-most open region for the given attribute.

The ``byname`` variants refer to attribute keys by their name. If no
attribute key with the given name exists, it will be created with
default properties. The basic variants take an attribute ID, e.g.
from :c:func:`cali_create_attribute()`.

Example:

.. code-block:: c

   #include <caliper/cali.h>

   /* Exports CustomAttribute="My great example" */
   cali_begin_string_byname("CustomAttribute", "My great example")

   /* Creates attribute key "myvar" with ASVALUE storage property */
   cali_id_t myvar_attr =
     cali_create_attribute("myvar", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

   /* Exports myvar=42 */
   cali_set_int(myvar_attr, 42);

   /* Closes CustomAttribute="My great example" */
   cali_end_byname("CustomAttribute");

Fortran annotation API
................................

The Fortran API is a thin wrapper around the C API. Fortran subroutine
names and semantics are identical to the respective C versions.

Fortran API signatures::

     subroutine cali_create_attribute(name, type, properties, id)
       character(len=*),        intent(in)  :: name
       integer,                 intent(in)  :: type
       integer,                 intent(in)  :: properties
       integer(kind=C_INT64_T), intent(out) :: id

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

     subroutine cali_end(id, err)
       integer(kind=C_INT64_T),     intent(in) :: id
       integer(kind(CALI_SUCCESS)), intent(out), optional :: err

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

Fortran API example
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

::

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


C++ annotation API
................................

The C++ annotation API is implemented in the class
:cpp:class:`cali::Annotation`.

.. doxygenclass:: cali::Annotation
   :project: caliper

Data tracking API
--------------------------------

Caliper also supports tracking allocated data. Doing so provides
advanced data-centric attributes, such as recording allocation events
and determining the containers for memory addresses provided by
services like libpfm. To take advantage of annotated memory
allocations, the :ref:`alloc <alloc-service>` service must be enabled
at runtime.

Memory allocation annotations are similar to code region annotations,
we can define labels for allocations using macros, which will use the
variable name of the given pointer as label for the memory
allocations.  We can either label 1-dimensional ranges of bytes using
`CALI_DATATRACKER_TRACK` or multi-dimensional ranges of specified
element sizes using `CALI_DATATRACKER_TRACK_DIMENSIONAL`.  The
following example shows both::

    void do_work(size_t M, size_t W, size_t N)
    {
        double *arrayA = (double*)malloc(N);
        CALI_DATATRACKER_TRACK(arrayA, N);

        double *matA =
             (double*)malloc(sizeof(double)*M*W);

        size_t num_dimensions = 2;
        size_t A_dims[] = {M,W};
        CALI_DATATRACKER_TRACK_DIMENSIONAL(
                    matA,
                    sizeof(double),
                    A_dims,
                    num_dimensions);
            ...

        CALI_DATATRACKER_FREE(arrayA);
        CALI_DATATRACKER_FREE(matA);
    }

API Reference
--------------------------------

.. doxygengroup:: AnnotationAPI
   :project: caliper
   :members:

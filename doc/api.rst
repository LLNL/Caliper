Annotation API
================================

Caliper provides source-code instrumentation APIs for C and C++ to
create and access Caliper context attributes. 

C++ annotation API
--------------------------------

The `cali::Annotation` class provides the C++ instrumentation interface.

.. cpp:class:: cali::Annotation

   #include <Annotation.h>

   Instrumentation interface to add and manipulate context attributes

   The Annotation class is the primary source-code instrumentation interface
   for Caliper. Annotation objects provide access to named Caliper context 
   attributes. If the referenced attribute does not exist yet, it will be 
   created automatically.

   Example:

   .. code-block:: c++

     cali::Annotation phase_ann("myprogram.phase");
     
     phase_ann.begin("Initialization");
       // ...
     phase_ann.end();

   This example creates an annotation object for the `myprogram.phase`
   attribute, and uses the `begin()` and `end()` methods to mark a section 
   of code where that attribute is set to "Initialization".

   Note that the access to a named context attribute through Annotation 
   objects is not exclusive: two different Annotation objects can reference and
   update the same context attribute.

   The Annotation class is *not* threadsafe; however, threads can safely
   access the same context attribute simultaneously through different 
   Annotation objects.

   .. cpp:function:: cali::Annotation::Annotation(const char* name, \
        int properties = 0)

      Constructor. Constructs an annotation object to manipulate the Caliper 
      context attribute `name`. If the attribute does not already exist,
      it will be created with the attribute properties specified in
      `properties`. See `cali_attr_properties` for a list of attribute
      properties.

  

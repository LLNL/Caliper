Concepts
================================

To utilize Caliper effectively, it is helpful to familiarize yourself
with the following concepts:


Attributes
--------------------------------

Attributes are the basic element in Caliper's data model. Essentially,
attributes are *key:value* pairs.

*Attribute keys* have a unique name, which serves as identifier for
attributes. The key also define the attributes' datatype, and carries
additional property flags and optional metadata.

Attributes are identified through their name. Therefore, it is
important to choose unique names for attributes. Caliper is designed
to let independent program components access any attribute only
through its name, without introducing compile-time data
dependencies. However, this makes inadvertent name conflicts difficult
to detect.

Datatypes
................................

The attribute key defines the datatype of an attribute. Only matching
values can be assigned; e.g. it is not possible to assign string
values to integer-type attribute keys.

Caliper supports integer (signed and unsigned), double-precision
floating point, pointer/address, boolean/logical, and string
datatypes. It is also possible to store raw binary data.

Hierarchical/stacked values 
................................

Values can be stacked to express hierarchical concepts. The
``begin/end`` function pairs in Caliper's APIs behave like a stack
interface: with ``begin``, a new value will be added on top of the
stack, while ``end`` removes the top-most value. For example, we can
create an attribute to track the call stack:

.. code-block:: c

   void bar()
   {
     cali_begin_string_byname("callstack", "bar");
     // ...
     cali_end_byname("callstack");     
   }
   
   void foo()
   {
     cali_begin_string_byname("callstack", "foo");
     bar();
     cali_end_byname("callstack");
   }

   void main()
   {
     cali_begin_string_byname("callstack", "main");
     bar();
     foo();
     cali_end_byname("callstack");
   }

Assuming we take a snapshot with each update of the ``callstack``
attribute, we will get the following sequence: ::

  callstack=main
  callstack=main/bar
  callstack=main
  callstack=main/foo
  callstack=main/foo/bar
  callstack=main/foo
  callstack=main

Stacks are *per-attribute*, i.e., each attribute key has its own,
independent stack. (In fact, thread-scope attributes (see below) have
a stack for each attribute on each thread).

To avoid stacking, use the ``set`` functions; these will just
overwrite the current (top-most) attribute value.

Note that stacking is not possible for attributes with ``ASVALUE``
storage (see below).


Properties
................................

Caliper defines properties that can be assigned to each attribute key.
These properties define the way the attribute is processed or stored
by Caliper.

Scope
  The attribute scope defines whether the attribute should be
  process-wide or thread-local. By default, an attribute is
  thread-local.
  
Storage representation
  By default, Caliper represents attributes on the blackboard and in
  snapshot records as a reference into a generalized context
  tree. With ``ASVALUE`` storage, the attribute will be explicitly
  stored as a key/value pair.

Event processing
  By default, Caliper executes event callback functions on attribute
  updates. With the ``SKIP_EVENTS`` property, the event processing can
  be disabled for the attribute.

Blackboard
--------------------------------

The *blackboard* is a runtime buffer that combines all active
attributes. Caliper data providers (e.g., annotated source-code
components) and Caliper data users can create, update, delete or read
attributes on the blackboard independently and in parallel. Thus, the
blackboard enables a global view of all active attributes without
creating explicit compile-time or link-time dependencies between
different data providers, or data providers and data users.



Snapshots
--------------------------------

A snapshot saves the blackboard contents at a specific point in the
execution of a target program. Snapshots can be triggered via
:cpp:func:`Caliper::push_snapshot` or
:cpp:func:`Caliper::pull_snapshot` methods. Snapshots can be triggered
asynchronously, independent of blackboard updates.

In addition to blackboard contents, Caliper can add *transient*
attributes to snapshots. This is done through the
:cpp:func:`Caliper::Events::snapshot` callback function, and is used
to add measurements such as timestamps to a snapshot. 
          

Developer guide
================================

This section describes internal APIs for Caliper service developers.

Caliper instance objects
--------------------------------

The Caliper API is accessed through objects of the `cali::Caliper`
class. The class constructor or the static
:cpp:func:`Caliper::instance` and
:cpp:func:`Caliper::sigsafe_instance` methods can create Caliper
instance objects. Example:

.. code-block:: c++

  #include <Caliper.h>

  int foo()
  {
    cali::Caliper c; // Create a Caliper instance object
    c.push_snapshot(CALI_SCOPE_THREAD, nullptr);

    cali::Caliper::instance().push_snapshot(CALI_SCOPE_THREAD, nullptr); // alternative
  }
  
Caliper instance objects contain thread-specific information. We
highly recommend creating Caliper objects only on the stack (i.e., as
non-static local variable within a function). In particular, *do
not* share Caliper objects between different threads.

Signal safety
--------------------------------

(Most of) Caliper's API is async-signal safe. This works as follows:

 * A signal handler function must request a Caliper instance object
   with the :cpp:func:`Caliper::sigsafe_instance` static method, and
   test if the object could be successfully obtained. The request will
   fail if:
   
   * The thread executing the signal handler is already
     executing a Caliper API function, or
   * Caliper has not been initialized, or Caliper's thread-local data
     has not been initialized yet for the thread executing the signal
     handler. Caliper needs to create a thread-local data object
     for each new thread. This can be done by creating a Caliper
     instance object in the new thread outside of a signal handler.
     
 * Caliper instance objects contain a flag that indicates whether the
   object was created in a signal handler. Caliper API functions
   deemed signal-safe, and callback functions invoked through these
   API functions, must check this flag using the
   :cpp:func:`Caliper::is_signal` method before executing any
   non-signal safe actions. If the flag is set, they must not execute
   such actions.

Note that compliance with Caliper's signal safety mechanics can't be
enforced by technical means, but we strongly encourage service developers
to follow these guidelines.

We also encourage developers to make sure their services function
within signal handlers as far as possible. Even so, it is still often
necessary to skip functionality when it can't be performed in a
signal-safe way; in this case it is good practice to count and report
the number of times the functionality had to be skipped.

Example:

.. code-block:: c++

  #include <Caliper.h>

  static unsigned n_snapshots_dropped = 0;
  static unsigned n_signals_dropped = 0;
  static unsigned n_signals = 0;
  
  using namespace cali;

  void async_signal_handler()
  {
    ++n_signals;
    
    // Request a signal-safe caliper instance object
    Caliper c = Caliper::sigsafe_instance();

    if (!c) {
      // Could not obtain a signal-safe Caliper object -> bail out
      ++n_signals_dropped;
      return;
    }

    // Now it's safe to perform signal-safe actions, e.g. push_snapshot()
    c.push_snapshot(CALI_SCOPE_THREAD, nullptr);
  }

  void snapshot_cb(Caliper* c, int scope, const EntryList*, EntryList* snapshot)
  {
    // Inside this Caliper callback function, check if it is safe
    // to perform non-sigsafe actions

    if (c->is_signal()) {
      // If functionality can't be performed in signal-safe manner, bail out

      ++n_snapshots_dropped;
      return;
    } else {
      // Memory allocations are not signal safe:
      // only run this code if we're not called from a signal handler
      
      int* buf = new int[size];
      do_stuff(buf);
      delete[] buf;
    }
  }

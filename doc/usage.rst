Getting started
================================

Caliper is an abstraction layer for a variety of performance
engineering tasks, such as tracing, profiling, monitoring, or on-line
adaptation. Caliper provides

* Annotation APIs for marking source-code regions or
  exporting arbitrary data in the form of key:value attributes.
  Annotations can be added (and removed) independent of each other;
  Caliper will automatically combine information from different
  annotations.
* Built-in *service* modules, which provide measurement data
  (e.g., timestamps), or perform specific functions such as
  recording profiles or traces.

Source-code annotations and Caliper service modules can be flexibly
combined to support a wide range of performance engineering use cases.
Specific actions to be performed (e.g., creating a trace or profile)
can be specified in a configuration profile at runtime.

Source-code annotations
--------------------------------

Caliper provides annotation APIs for C, C++, and Fortran.
The annotation commands fulfill two functions:

(1) They provide contextual information, such as the name of a code
    region or an iteration number.
(2) They serve as hooks for additional actions (e.g., taking
    performance measurements). 

The specific action performed is determined by a runtime configuration
profile. By default, the annotations perform no actions other than
updating the Caliper-internal *blackboard* buffer.
    
The following example marks "initialization" and "loop" phases in a
C++ code, and exports the main loop's current iteration counter.

.. code-block:: c++
                
    #include <Annotation.h>

    int main(int argc, char* argv[])
    {
        // Mark begin of "initialization" phase
        cali::Annotation
            init_ann = cali::Annotation("initialization").begin();
        // perform initialization tasks
        int count = 4;
        // Mark end of "initialization" phase
        init_ann.end();

        if (count > 0) {
            // Mark begin of "loop" phase. The scope guard will
            // automatically end it at the end of the C++ scope
            cali::Annotation::Guard 
                g_loop( cali::Annotation("loop").begin() );

            // Create "iteration" attribute to export the iteration count
            cali::Annotation iteration_ann("iteration");

            for (int i = 0; i < count; ++i) {
                // Export current iteration count under "iteration"
                iteration_ann.set(i);

                // A Caliper snapshot taken at this point will contain
                // { "loop", "iteration"=<i> }
            }

            // Clear the "iteration" attribute (otherwise, snapshots taken
            // after the loop will still contain the last "iteration" value)
            iteration_ann.end();
        }
    }

See :doc:`api` for examples in C and Fortran.

Caliper-enabled tools will now be able to access the information
provided by the source-code annotations at runtime by taking
snapshots. The source-code annotations also provide hooks to enable
various runtime actions in built-in Caliper service modules, such as
triggering snapshots or writing traces.

Build and link Caliper programs
--------------------------------

To use Caliper, add annotation statements to your program and link it
against the Caliper library. Programs must be linked with the Caliper
runtime (libcaliper) and infrastructure libraries (libcaliper-common). ::
  
    CALIPER_LIBS = -L$(CALIPER_DIR)/lib -lcaliper

See :doc:`link` for more information.

Configuring and Running Caliper programs
------------------------------------------

By default, Caliper annotation commands perform no actions other than
updating a Caliper-internal *blackboard buffer* . However, we can
connect a Caliper-enabled third-party tool to the program, or enable
built-in Caliper "service" modules to take measurements and collect
data.

As an example, Caliper's built-in `trace` configuration profiles
trigger and write snapshots whenever any or specific attributes are
updated, generating a snapshot trace. A configuration profile can be
selected with the `CALI_CONFIG_PROFILE` environment variable:

.. code-block:: sh
                
    $ CALI_CONFIG_PROFILE=thread-trace ./cali-basic
    == CALIPER: Registered pthread service
    == CALIPER: Registered recorder service
    == CALIPER: Registered timestamp service
    == CALIPER: Initialized
    == CALIPER: Wrote 36 records.
    == CALIPER: Finished

With this configuration, Caliper will take a snapshot for each
attribute update performed by the annotation commands, calculate the
time spent in each of the annotated phases, and write the results in
form of a snapshot trace to a `.cali` file in the current working
directory.

Analyzing Data
--------------------------------

Use the `cali-query` tool to filter, aggregate, or print the recorded
traces. For example, the following command will show us the time spent
in the "initialization" phase, in the entire "loop" phase, and in each
iteration of the example program: 

.. code-block:: sh
                
    $ ls *.cali
    160219-095419_5623_LQfNQTNgpqdM.cali
    $ cali-query --table \
          --print-attributes=initialization:loop:iteration:time.inclusive.duration \
          160219-095419_5623_LQfNQTNgpqdM.cali
    initialization loop iteration time.inclusive.duration
    true                                             4437
                   true
                   true         0                     179
                   true         1                      14
                   true         2                      11
                   true         3                      12
                   true                               435 

Where to go from here?
--------------------------------

The :doc:`concepts` section explains Caliper's underlying concepts
better.

Much of Caliper's functionality is implemented by built-in "services",
which can be enabled or disabled as needed. Refer to the
:doc:`services` section to learn about functionality they provide.

The :doc:`api` section in the documentation provides reference
documentation for Caliper's C, C++, and Fortran annotation APIs.
Finally, the :doc:`tools` section documents the command-line tools
used to analyze and export the data streams produced by Caliper's
tracing and profiling services.

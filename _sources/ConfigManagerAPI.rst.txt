ConfigManager API reference
===============================

Caliper provides several built-in performance measurement and reporting
configurations. These can be activated from within a program with the
`ConfigManager` API using a short configuration string. Configuration
strings can be hard-coded in the program or provided by the user in some
form, e.g. as a command-line parameter or in the programs's configuration
file.

To access and control the built-in configurations, create a
:cpp:class:`cali::ConfigManager` object. Add a configuration string with `add()`,
start the requested configuration channels with `start()`, and trigger
output with `flush()`:

.. code-block:: c++

   #include <caliper/cali-manager.h>

   int main(int argc, char* argv[])
   {
      cali::ConfigManager mgr;

      if (argc > 1)
         mgr.add(argv[1]);
      if (mgr.error())
         std::cerr << "Config error: " << mgr.error_msg() << std::endl;
      // ...
      mgr.start(); // start requested performance measurement channels
      // ... (program execution)
      mgr.flush(); // write performance results
   }


API Reference
-------------------------------

.. doxygengroup:: ControlChannelAPI
   :project: caliper
   :members:

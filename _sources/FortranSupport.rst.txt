Fortran support
================================

Caliper provides Fortran wrappers for the annotation and ConfigManager
APIs. To build Caliper with Fortran support, enable the ``WITH_FORTRAN``
option in the CMake configuration:

.. code-block:: sh

  $ cmake -DWITH_FORTRAN=On ..

Using the Fortran module
--------------------------------

The Fortran wrapper module requires Fortran 2003, and Caliper must be built
with the same compiler as the target program.

The Caliper fortran module is installed in ``include/caliper/fortran/`` in the
Caliper installation directory. Simply add this directory to the application
include path and import it with ``use caliper_mod`` where needed.
Remember to also link libcaliper to the target program.

Caliper Fortran API
--------------------------------

The Caliper Fortran API supports a subset of the C and C++ annotation APIs.
The simplest options are the :cpp:func:`cali_begin_region()` and
:cpp:func:`cali_end_region()` functions.

The Fortran API also supports the Caliper ConfigManager API (:doc:`ConfigManagerAPI`).
The example in examples/apps/fortran-example.f demonstrates the annotation and
ConfigManager APIs for Fortran:

.. literalinclude:: ../../examples/apps/fortran-example.f
   :language: Fortran

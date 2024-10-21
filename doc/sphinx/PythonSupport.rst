Python support
==============

Caliper provides Python bindings based on `pybind11 <https://pybind11.readthedocs.io/en/stable/>`_
for the annotation and :code:`ConfigManager` APIs. To build Caliper with Python support, enable
the :code:`WITH_PYTHON_BINDINGS` option in the CMake configuration:

.. code-block:: sh

    $ cmake -DWITH_PYTHON_BINDINGS=On ..

Using the Python module
-----------------------

The Python module requires pybind11 and an installation of Python that both supports
pybind11 and provides development headers (e.g., :code:`Python.h`) and libraries
(e.g., :code:`libpython3.8.so`).

The Caliper Python module is installed in either :code:`lib/pythonX.Y/site-packages/` and/or
:code:`lib64/pythonX.Y/site-packages` in the Caliper installation directory. In these paths,
:code:`X.Y` corresponds to the major and minor version numbers of the Python installation used.
Additionally, :code:`lib/` and :code:`lib64/` will be used in accordance with the configuration
of the Python installed. To better understand the rules for where Python modules are installed, 
see `this thread <https://discuss.python.org/t/understanding-site-packages-directories/12959>`_
from the Python Software Foundation Discuss.

To use the Caliper Python module, simply add the directories above to :code:`PYTHONPATH` or
:code:`sys.path`. Note that the module will be automatically added to :code:`PYTHONPATH` when
loading the Caliper package with Spack if the :code:`python` variant is enabled.
The module can then be imported with :code:`import pycaliper`.

Caliper Python API
------------------

The Caliper Python API supports a significant subset of the C and C++ annotation APIs.
The simplest options are the :code:`pycaliper.begin_region()` and :code:`pycaliper.end_region()`
functions. Caliper's Python API also provides the :code:`pycaliper.annotate_function` decorator
as a higher-level way of annotating functions.

The Python API also supports the Caliper :code:`ConfigManager` API (:doc:`ConfigManagerAPI`).
The example is examples/apps/py-example.py demonstrates the annotation and
:code:`ConfigManager` APIs for Python:

.. literalinclude:: ../../examples/apps/py-example.py
   :language: Python
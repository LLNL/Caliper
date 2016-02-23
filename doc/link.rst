Linking Caliper programs
================================

Typically, all that is needed to create a Caliper-enabled program is
to link it with the Caliper runtime library, which resides in
``libcaliper.so``. An example link command for a C++ program built
with g++ could look like this: ::
  
  CALIPER_DIR  = /path/to/caliper/installation

  g++ -o target-program $(OBJECTS) -L$(CALIPER_DIR)/lib -lcaliper

Note that Caliper is a C++ library: to link a C or Fortran program
with Caliper, either use a C++ linker, or manually add the C++
standard library to the link line (e.g., with ``-lstdc++``).

MPI
--------------------------------

Caliper provides wrappers for MPI calls in the separate
``libcaliper-mpiwrap.so`` library. It is not strictly necessary to use
the wrapper library for MPI programs with Caliper
annotations. However, it will make Caliper's behavior more
multi-process friendly, e.g. by reducing log output on most ranks. The
wrapper library *is* required to use Caliper's MPI service.

To use the wrapper library, add it before the Caliper libraries in a
link command: ::

  mpicxx -o target-program $(OBJECTS) -L$(CALIPER_DIR)/lib -lcaliper-mpiwrap -lcaliper

Fortran
--------------------------------

Caliper provides a Fortran wrapper module in source code form under
``share/fortran/caliper.f90`` in the Caliper installation
directory. This way, we avoid potential incompatibilities between
compilers used to build Caliper and the target program.
We recommend to simply add the Caliper module to the target
program. An example Makefile may look like this: ::

  F90         = gfortran
  
  CALIPER_DIR = /path/to/caliper/installation
  OBJECTS     = main.o
  
  target-program : $(OBJECTS) caliper.o
      $(F90) -o target-program $(OBJECTS) -L$(CALIPER_DIR)/lib -lcaliper -lstdc++

  %.o : %.f90 caliper.mod
      $(F90) -c $<

  caliper.mod : caliper.o
      
  caliper.o : $(CALIPER_DIR)/share/fortran/caliper.f90
      $(F90) -std=f2003 -c $<

Note that it is necessary to link in the C++ standard library as
well. With ``gfortran``, add ``-lstdc++``: ::

  gfortran -o target-program *.o -L/path/to/caliper/lib -lcaliper -lstdc++
  
With Intel ``ifort``, you can use the ``-cxxlib`` option: ::

  ifort -o target-program *.o -cxxlib -L/path/to/caliper/lib -lcaliper

The wrapper module uses Fortran 2003 C bindings. Thus, it requires a
Fortran 2003 compatible compiler to build, but should be usable with
any reasonably "modern" Fortran code. More work may be required to
integrate it with Fortran 77 codes.


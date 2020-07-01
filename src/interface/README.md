# Fortran and C interface wrappers

The Fortran and C interface is generated with Shroud: https://github.com/LLNL/shroud/.
Make sure to use a recent version (post July 1st, 2020).

To re-generate the interface, use

   $ shroud --outdir-c-fortran c_fortran --outdir-yaml yaml caliper_shroud.yaml

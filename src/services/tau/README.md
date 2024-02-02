TAU Service
--------------------------------

The TAU service publishes Caliper trace or aggregation data to TAU
profiles or traces.

To enable the TAU service when configuring CMake for Caliper, add 
the following CMake variables:

```
-DWITH_TAU=TRUE \
-DTAU_PREFIX=/path/to/tau2 \
-DTAU_MAKEFILE=$TAU_MAKEFILE \
-DTAU_ARCH=x86_64 \
```

Where `TAU_PREFIX` is set to the root TAU installation directory,
`TAU_MAKEFILE` specifies the TAU configuration you want to use, and
`TAU_ARCH` specifies the compute platform (when cross-compiling).

For example, when configuring on a system where TAU is configured with
pthread, papi and pgi compiler support, you might specify:

```
-DWITH_TAU=TRUE \
-DTAU_PREFIX=$HOME/src/tau2 \
-DTAU_MAKEFILE=$HOME/src/tau2/craycnl/lib/Makefile.tau-pgi-pthread-papi \
-DTAU_ARCH=craycnl \
```

For any/all questions please contact tau-bugs@cs.uoregon.edu.

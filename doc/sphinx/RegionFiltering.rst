Region filtering
=======================================

For event-based measurement configurations like runtime-report or
hatchet-region-profile, you can specify filters to limit measurements
to certain regions using two ConfigManager options:

include_regions
    Only take measurements for regions with the given pattern.

exclude_regions
    Skip measurements for regions with the given pattern.

The options take a list of region patterns. There are three pattern types:

match
    An exact match. For example, `match(this_function)`
    matches `this_function`.

startswith
    Match the start of the region name, e.g. `startswith(mylib_)`
    matches any region starting with `mylib_`.

regex
    Match a regular expression in ECMAScript grammar. E.g., `regex(.*loop.*)`
    matches any region with `loop` in the name.

The default pattern is `match`. As an example, the following option
specification measures `my_function`, any region starting with `mylib_` or
`MPI_`, and any region with `loop` in the name::

    include_regions=my_function,startswith(MPI_,mylib_),regex(.*loop.*)

Examples
---------------------------------------

Recall a runtime report for the `cxx-example` program provided with Caliper::

    $ CALI_CONFIG=runtime-report ./examples/apps/cxx-example
    Path       Min time/rank Max time/rank Avg time/rank Time %
    main            0.000096      0.000096      0.000096  4.541154
      mainloop      0.000060      0.000060      0.000060  2.838221
        foo         0.000674      0.000674      0.000674 31.882687
      init          0.000013      0.000013      0.000013  0.614948

We can use the `exclude_regions` option to exclude the `init` region from
measurements::

    $ CALI_CONFIG=runtime-report,exclude_regions=init ./examples/apps/cxx-example
    Path       Min time/rank Max time/rank Avg time/rank Time %
    main            0.000113      0.000113      0.000113  5.188246
      mainloop      0.000118      0.000118      0.000118  5.417815
        foo         0.000675      0.000675      0.000675 30.991736

We can also limit measurements to the `foo` region. The full path to `foo`
still appears in the output. However, performance measurements will only be
taken when entering and exiting `foo`::

    $ CALI_CONFIG=runtime-report,include_regions=foo ./examples/apps/cxx-example
    Path       Min time/rank Max time/rank Avg time/rank Time %
    main
      mainloop      0.001390      0.001390      0.001390 66.539014
        foo         0.000699      0.000699      0.000699 33.460986

Any measurement values taken when entering `foo` are assigned to its enclosing
region - `mainloop` in this case - which is why we still see measurement values
for the `mainloop` region here. However, these do not represent the actual time
spent in `mainloop`.

We can use a pattern like `startswith(main)` to include `main` and `mainloop`.
Be careful to wrap the option values in quotes to prevent them from being 
misinterpreted by the config parser::

    $ CALI_CONFIG="runtime-report,include_regions=\"startswith(main)\"" ./examples/apps/cxx-example
    Path       Min time/rank Max time/rank Avg time/rank Time %
    main            0.000112      0.000112      0.000112  5.177994
      mainloop      0.000773      0.000773      0.000773 35.737402

# Caliper Service Templates

The service templates serve as templates, demos, and documentation for aspiring
Caliper service developers.

## MeasurementService

The MeasurementService template demonstrates a performance measurement service
(e.g., for reading hardware counters, timestamps, and other kinds of
performance data at runtime). It implements the `snapshot` callback for adding
data to Caliper snapshot records.

To try it out, build Caliper with testing turned on:

    $ mkdir build && cd build
    $ cmake -DBUILD_TESTING=On ..
    $ make
    $ make cxx-example

Try it using the cxx-example app:

    $ CALI_SERVICES_ENABLE=event,measurement_template,trace,report \
      CALI_MEASUREMENT_TEMPLATE_NAMES=test \
      CALI_REPORT_CONFIG="select max(measurement.val.test),sum(measurement.test) group by prop:nested format tree" \
        ./examples/apps/cxx-example none
    Path         max#measurement.val.test sum#measurement.test
    main                              708                  260
      mainloop                        684                  220
        foo                           648                   72
      init                            272                   44

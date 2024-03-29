Connection to third-party tools
================================

Caliper provides bindings to export Caliper annotations to third-party
tools. Currently, Nvidia's NVProf/NSight and Intel VTune Amplifier are
supported.


NVidia NVProf/NSight
--------------------------------

The `nvtx` service translates Caliper annotations into NVTX ranges
(Nvidia's source-code annoatation interface). Caliper-annotated
regions will then be visible in the "Markers and Ranges" section of
the NVprof or NSight timeline display.

To use the NVprof bindings, enable the `nvtx` service and run the
target application under nvprof:

.. code-block:: sh

    $ CALI_SERVICES_ENABLE=nvtx nvprof <nvprof-options> app <app-options>

Then, load the measurement result file created by nvprof in nvvp (use
the -o flag for nvprof to write output into a file).

By default, Caliper exports all regions with the NESTED attribute
property. This includes the regions created by Caliper's annotation
macros (CALI_CXX_MARK_FUNCTION, CALI_MARK_BEGIN/END
etc.). Alternatively, the region attributes to export can be specified
explicitly in the `CALI_NVTX_TRIGGER_ATTRIBUTES` configuration
variable.

CALI_NVTX_TRIGGER_ATTRIBUTES
    Specify which attributes should be exported as NVTX
    ranges. Comma-separated list.


Intel VTune
--------------------------------

The `vtune` service translates Caliper annotations into Intel "itt"
markups that can be shown as "tasks" in the Intel VTune GUI.

To use the vtune bindings, run the target application in VTune with
the `vtune` service enabled. To do so in the VTune GUI, do the
following:

* In the "Analysis Target" tab, go to the "User-defined environment
  variables" section, and add an entry setting "CALI_SERVICES_ENABLE"
  to "vtune"
* In the "Analysis Type" tab, check the "Analyze user tasks, events,
  and counters" checkbox.

Caliper-annotated regions will then be visible as "tasks" in the VTune
analysis views.

By default, Caliper exports all regions with the NESTED attribute
property. Alternatively, the region attributes to export can be
specified explicitly in the `CALI_VTUNE_TRIGGER_ATTRIBUTES`
configuration variable.

CrayPAT
--------------------------------

The `craypat` service translates Caliper annotations into CrayPAT regions.
This API is only available in recent (2022+) CrayPAT versions. It also
only works with the "trace" call stack mode in CrayPAT.

By default, the `craypat` service will attempt to initialize CrayPAT internally
and record a CrayPAT region profile. Alternatively, you can use the `craypat`
service in conjunction with `pat_run` to set additional recording options. In
this case, make sure to set ``PAT_RT_CALLSTACK_MODE=trace``::

  $ PAT_RT_CALLSTACK_MODE=trace CALI_SERVICES_ENABLE=craypat pat_run -w -E PAPI_TOT_CYC ./examples/apps/cxx-example
  CrayPat/X:  Version 22.06.0 Revision 4b5ab6256  05/21/22 02:04:43
  Experiment data directory written:
  /<path>/cxx-example+91423-10t
  $ pat_report ./cxx-example+91423-10t
  [...]
  Table 1:  Profile by Function Group and Function

      Time% |     Time | Imb. |  Imb. | Calls | Group
            |          | Time | Time% |       |  Function

     100.0% | 0.000716 |   -- |    -- |   7.0 | Total
  |------------------------------------------------------
  |  100.0% | 0.000716 |   -- |    -- |   7.0 | USER
  ||-----------------------------------------------------
  ||  89.2% | 0.000639 |   -- |    -- |   4.0 | [R]foo
  ||   4.7% | 0.000034 |   -- |    -- |   1.0 | [R]main
  ||   3.9% | 0.000028 |   -- |    -- |   1.0 | [R]mainloop
  ||   2.1% | 0.000015 |   -- |    -- |   1.0 | [R]init
  |======================================================
  [...]

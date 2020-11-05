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

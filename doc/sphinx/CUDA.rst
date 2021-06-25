CUDA profiling
=======================================

Caliper can profile CUDA API functions and CUDA device-side activities like
kernel executions and memory copies. This requires Caliper to be built with
CUpti support (`-DWITH_CUPTI=On`).

Profiling CUDA host-side API functions
---------------------------------------

The `profile.cuda` option is available for many ConfigManager configs like
`runtime-report`, `hatchet-region-profile`, and `spot`. It intercepts host-side
CUDA API functions like ``cudaMemcpy`` and ``cudaDeviceSynchronize``.
The example below shows `runtime-report` output with the `profile.cuda` option
enabled. We can see the time spent in CUDA functions like ``cudaMemcpy`` and
``cudaLaunchKernel``::

    $ CALI_CONFIG=runtime-report,profile.cuda lrun -n 4 ./tea_leaf
    Path                        Min time/rank Max time/rank Avg time/rank Time %
    timestep_loop                    0.000175      0.000791      0.000345  0.002076
      summary                        0.000129      0.000153      0.000140  0.000846
        cudaMemcpy                   0.000414      0.000422      0.000418  0.002516
        cudaLaunchKernel             0.000247      0.000273      0.000256  0.001542
      total_solve                    0.000105      0.000689      0.000252  0.001516
        reset                        0.000160      0.000179      0.000169  0.001021
          internal_halo_update       0.000027      0.000029      0.000028  0.000167
          halo_update                0.000028      0.000031      0.000029  0.000176
          halo_exchange              0.000496      0.000812      0.000607  0.003654
            cudaMemcpy               0.001766      0.001792      0.001779  0.010713
            cudaLaunchKernel         0.000418      0.000450      0.000430  0.002588
          cudaLaunchKernel           0.000097      0.000126      0.000106  0.000640
    ...

Profiling CUDA activities
---------------------------------------

The `cuda-activity-report` and `cuda-activity-profile` configs record the time
spent in CUDA activities (e.g. kernel executions or memory copies) on the CUDA
device. The GPU times are mapped to the Caliper regions that launched those
GPU activities. Here is example output for `cuda-activity-report`::

    $ CALI_CONFIG=cuda-activity-report lrun -n 4 ./tea_leaf
    Path                        Avg Host Time Max Host Time Avg GPU Time Max GPU Time GPU %
    timestep_loop                   17.052183     17.053008    12.020218    12.037409   70.490786
      total_solve                   17.048899     17.049464    12.019799    12.036989   70.501909
        solve                       17.026991     17.027000    12.009868    12.027047   70.534294
          dot_product                0.004557      0.006057
          cudaMalloc                 0.000050      0.000055
          internal_halo_update       0.051974      0.052401
          halo_update                0.110023      0.111015
          halo_exchange             15.154157     15.184635     0.621857     0.625657    4.103542
            cudaMemcpy              12.398854     12.422384     0.234947     0.235587    1.894913
            cudaLaunchKernel         1.426069      1.440035     0.386910     0.390767   27.131206
          cudaMemcpy                 0.497959      0.503879     0.003377     0.003433    0.678115
          cudaLaunchKernel           0.772027      0.793401    11.384634    11.402917 1474.641141

For each Caliper region, we now see the time spent on the CPU ("Avg/Max Host
Time") and the aggregate time on the GPU for activities launched from
this region ("Avg/Max GPU Time"). Note how the ``cudaLaunchKernel`` call in
the last row of the output has ~0.78 seconds of CPU time associated with it,
but ~11.38 seconds of GPU time - these 11.38 seconds reflect the amount of GPU
activities launched asynchronously from this region. Most CPU time in the
example output is actually spent in ``cudaMemcpy`` under ``halo_exchange``
- we can deduce that much of this is actually synchronization time spent
waiting until the GPU kernels are complete.

The "GPU %" metric shows the fraction of (wall-clock) CPU time in which there
are GPU activities, and represents GPU utilization inside a Caliper region.
The percentage is based on the total inclusive CPU time and GPU activities for
the region and it sub-regions. The "GPU %" for the top-level region
(``timestep_loop``) represents the GPU utilization of the entire program.

The definition of the reported metrics is as follows:

Avg Host Time
    Inclusive time (seconds) in the Caliper region on the Host (CPU).
    Typically the wall-clock time. Average value across MPI ranks.

Max Host Time
    Inclusive time (seconds) in the Caliper region on the Host (CPU).
    Typically the wall-clock time. Maximum value among all MPI ranks.

Avg GPU Time
    Inclusive total time (seconds) of activities executing on the GPU
    launched from the Caliper region. Average across MPI ranks.

Max GPU Time
    Inclusive total time (seconds) of activities executing on the GPU
    launched from the Caliper region. Maximum value among all MPI ranks.

GPU %
    Fraction of total inclusive GPU time vs. CPU time. Typically
    represents the GPU utilization in the Caliper region.

Show GPU kernels
---------------------------------------

Use the `show_kernels` option in `cuda-activity-report` to distinguish
individual CUDA kernels::

    $ CALI_CONFIG=cuda-activity-report,show_kernels lrun -n 4 ./tea_leaf
    Path                        Kernel                                           Avg Host Time Max Host Time Avg GPU Time Max GPU Time GPU %
    timestep_loop
     |-                                                                              17.068956     17.069917     0.239392     0.240725 1.402501
     |-                         device_unpack_top_buffe~~le*, double*, int, int)                                 0.091051     0.092734
     |-                         device_tea_leaf_ppcg_so~~ const*, double const*)                                 5.409844     5.419096
     |-                         device_tea_leaf_ppcg_so~~t*, double const*, int)                                 5.316101     5.320777
     |-                         device_pack_right_buffe~~le*, double*, int, int)                                 0.112455     0.113198
     |-                         device_pack_top_buffer(~~le*, double*, int, int)                                 0.092634     0.092820
   (...)
     |-                         device_pack_bottom_buff~~le*, double*, int, int)                                 0.098929     0.099095
      summary
       |-                                                                             0.000881      0.000964     0.000010     0.000011 1.179024
       |-                       device_field_summary_ke~~ble*, double*, double*)                                 0.000325     0.000326
       |-                       void reduction<double, ~~N_TYPE)0>(int, double*)                                 0.000083     0.000084
        cudaMemcpy                                                                    0.000437      0.000457     0.000010     0.000011 2.376874
        cudaLaunchKernel
         |-                                                                           0.000324      0.000392
         |-                     device_field_summary_ke~~ble*, double*, double*)                                 0.000325     0.000326
         |-                     void reduction<double, ~~N_TYPE)0>(int, double*)                                 0.000083     0.000084

We now see the GPU time in each kernel. The display is "inclusive", that is,
for each Caliper region, we see kernels launched from this region as well as
all regions below it. Under the top-level region (``timestep_loop``),
we see the total time spent in each CUDA kernel in the program.

Profiling memory copies
---------------------------------------

The `cuda.memcpy` option shows the amount of data copied between CPU and GPU
memory::

    $ CALI_CONFIG=cuda-activity-report,cuda.memcpy lrun -n 4 ./tea_leaf
    Path                        Avg Host Time Max Host Time Avg GPU Time Max GPU Time GPU %       Copy CPU->GPU (avg) Copy CPU->GPU (max) Copy GPU->CPU (avg) Copy GPU->CPU (max)
    timestep_loop                   17.098644     17.100283    12.020492    12.040316   70.300846
      total_solve                   17.094882     17.095510    12.020072    12.039896   70.313864
        solve                       17.072250     17.072253    12.010138    12.029962   70.348891
          dot_product                0.001489      0.001684
          cudaMalloc                 0.000050      0.000054
          internal_halo_update       0.052739      0.053484
          halo_update                0.110009      0.111008
          halo_exchange             15.194774     15.220273     0.622893     0.629519    4.099388
            cudaMemcpy              12.405958     12.430534     0.234917     0.235315    1.893578          902.517616          902.517616          902.469568          902.469568
            cudaLaunchKernel         1.431387      1.463419     0.387976     0.394445   27.104925
          cudaMemcpy                 0.493047      0.494699     0.003369     0.003393    0.683394            0.040320            0.040320            0.019232            0.019232

In the example, there were about 902 Megabytes copied from CPU to GPU memory
and back. There are four new metrics:

Copy CPU->GPU (avg)
    Data copied (Megabytes) from CPU to GPU memory. Average across MPI ranks.

Copy CPU->GPU (max)
    Data copied (Megabytes) from CPU to GPU memory. Maximum among MPI ranks.

Copy GPU->CPU (avg)
    Data copied (Megabytes) from GPU to CPU memory. Average across MPI ranks.

Copy GPU->CPU (max)
    Data copied (Megabytes) from GPU to CPU memory. Maximum among MPI ranks.

Profiles in JSON or CALI format
---------------------------------------

The `cuda-activity-profile` config records data similar to
`cuda-activity-report` but writes it in machine-readable JSON or Caliper's
".cali" format for processing with `cali-query`. By default, it writes the
`json-split` format that can be read by Hatchet::

    $ CALI_CONFIG=cuda-activity-profile lrun -n 4 ./tea_leaf
    $ ls *.json
    cuda_profile.json

Other formats can be selected with the `output.format` option. Possible
values:

cali
    The Caliper profile/trace format for processing with `cali-query`.

hatchet
    JSON format readable by Hatchet. See :ref:`json-split-format`.

json
    Easy-to-parse list-of-dicts style JSON. See :ref:`json-format`.

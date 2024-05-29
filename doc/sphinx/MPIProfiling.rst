MPI Profiling
================================

Caliper's built-in profiling recipes support MPI natively and automatically
aggregate performance data across all MPI ranks. In addition, Caliper provides
MPI performance statistics.

MPI Function Profiling
--------------------------------

The `mpi-report` config recipe lists the number of invocations and the time spent
in each MPI function (min/max/avg across MPI ranks). It works similar to the
mpiP profiling tool. The first row shows the time outside of MPI (i.e., the
computation time of the program)::

    $ CALI_CONFIG=mpi-report srun -n 8 ./lulesh2.0
    Function                Count (min) Count (max) Time (min) Time (max) Time (avg) Time %
                                    446         518   0.315387   0.415731   0.353483 83.299370
    MPI_Allreduce                    10          11   0.000281   0.068973   0.045038 10.613409
    MPI_Wait                        107         177   0.000795   0.032157   0.014788  3.484918
    MPI_Barrier                       2           2   0.000051   0.007671   0.005110  1.204122
    MPI_Isend                       107         177   0.002300   0.002799   0.002571  0.605904
    MPI_Waitall                      31          31   0.000482   0.001858   0.001149  0.270677
    MPI_Comm_split                    2           2   0.000176   0.001925   0.000999  0.235499
    MPI_Irecv                       107         177   0.000446   0.000767   0.000631  0.148605
    MPI_Bcast                         4           4   0.000054   0.000500   0.000436  0.102674
    MPI_Reduce                        1           1   0.000032   0.000296   0.000072  0.017057
    MPI_Comm_dup                      1           1   0.000038   0.000066   0.000052  0.012178
    MPI_Comm_free                     2           2   0.000012   0.000015   0.000013  0.003020
    MPI_Get_library_version           1           1   0.000007   0.000010   0.000008  0.001972
    MPI_Gather                        1           1   0.000020   0.000020   0.000020  0.000594

The `profile.mpi` option is available for most built-in profiling recipes, such as
`runtime-report` or `hatchet-region-profile`. It shows the time spent in MPI functions
within each Caliper region::

    $ CALI_CONFIG=runtime-report,profile.mpi srun -n 8 ./lulesh2.0
    Path                                       Min time/rank Max time/rank Avg time/rank Time %
    main                                            0.007467      0.007918      0.007664  1.775109
      CommRecv                                      0.000036      0.000068      0.000044  0.010198
        MPI_Irecv                                   0.000036      0.000076      0.000044  0.010223
      CommSend                                      0.000045      0.000053      0.000047  0.010781
        MPI_Isend                                   0.000597      0.000621      0.000608  0.140909
        MPI_Waitall                                 0.000015      0.000020      0.000016  0.003809
      CommSBN                                       0.000035      0.000042      0.000037  0.008527
        MPI_Wait                                    0.000027      0.000286      0.000147  0.034127
      MPI_Barrier                                   0.000013      0.000104      0.000065  0.014967
      lulesh.cycle                                  0.000212      0.000252      0.000228  0.052810
        TimeIncrement                               0.000085      0.000107      0.000091  0.021138
          MPI_Allreduce                             0.000210      0.071229      0.046590 10.791564
        LagrangeLeapFrog                            0.000263      0.000408      0.000320  0.074115
          LagrangeNodal                             0.004715      0.005330      0.005034  1.165980
            CalcForceForNodes                       0.000624      0.000747      0.000694  0.160774
              CommRecv                              0.000242      0.000287      0.000265  0.061496
                MPI_Irecv                           0.000240      0.000332      0.000280  0.064767
              CalcVolumeForceForElems               0.001827      0.002038      0.001919  0.444573
                IntegrateStressForElems             0.034616      0.038880      0.036624  8.483035
                CalcHourglassControlForElems        0.095108      0.102434      0.098921 22.912601
                  CalcFBHourglassForceForElems      0.062848      0.071650      0.067722 15.686204
              CommSend                              0.000838      0.000949      0.000890  0.206152
                MPI_Isend                           0.000899      0.001126      0.000999  0.231382
                MPI_Waitall                         0.000140      0.000298      0.000216  0.049950
              CommSBN                               0.000558      0.000692      0.000615  0.142361
                MPI_Wait                            0.000334      0.018442      0.008193  1.897615
            CommRecv                                0.000044      0.000249      0.000152  0.035190
              MPI_Irecv                             0.000042      0.000281      0.000162  0.032884
            CommSend                                0.000086      0.001056      0.000560  0.129665
              MPI_Isend                             0.000067      0.000525      0.000327  0.066173
              MPI_Waitall                           0.000043      0.002110      0.000808  0.187086
    (...)

Message statistics
................................

The `mpi.message.count` and `mpi.message.size` options show the number of
messages and transferred bytes for both point-to-point and collective communication
operations.

MPI Function filtering
................................

You can use the `mpi.include` and `mpi.exclude` to explicitly select or
filter out MPI operations to capture. This is a more efficient option than
filtering MPI functions with the name-based `include_regions` or
`exclude_regions` option. As an example, we can use `mpi.include` to only
measure `MPI_Allreduce`::

    $ CALI_CONFIG=runtime-report,profile.mpi,mpi.include=MPI_Allreduce
    Path                                       Min time/rank Max time/rank Avg time/rank Time %
    main                                            0.007588      0.008178      0.007737  1.834651
      CommRecv                                      0.000024      0.000034      0.000029  0.006834
      CommSend                                      0.000551      0.000631      0.000594  0.140933
      CommSBN                                       0.000019      0.000046      0.000031  0.007338
      lulesh.cycle                                  0.000218      0.000254      0.000233  0.055250
        TimeIncrement                               0.000088      0.000111      0.000098  0.023188
          MPI_Allreduce                             0.000180      0.066283      0.042576 10.095752
        LagrangeLeapFrog                            0.000278      0.000364      0.000324  0.076719
          LagrangeNodal                             0.004838      0.005261      0.005013  1.188590
            CalcForceForNodes                       0.000622      0.000839      0.000740  0.175528
              CommRecv                              0.000056      0.000076      0.000065  0.015465
    (...)


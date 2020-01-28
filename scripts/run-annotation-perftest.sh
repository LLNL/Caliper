#!/bin/bash

depth=20
width=200

EXE=`pwd`/test/cali-annotation-perftest
ARGS="-d ${depth} -w ${width}"

# Run Caliper annotation benchmarks. Designed to run on LLNL quartz (36 cores/node)

for threads in 1 4 36
do
    # Annotations only, no active measurement config
    OMP_NUM_THREADS=${threads} \
        ${EXE} ${ARGS} -P spot,perftest.experiment=annotations

    # Aggregation (as used in e.g. runtime-report)
    OMP_NUM_THREADS=${threads} \
    CALI_SERVICES_ENABLE=aggregate,event,timestamp \
    CALI_EVENT_ENABLE_SNAPSHOT_INFO=false \
    CALI_TIMER_SNAPSHOT_DURATION=true \
        ${EXE} ${ARGS} -P spot,perftest.experiment=aggregation

    # Trace w/ snapshot info
    OMP_NUM_THREADS=${threads} \
    CALI_SERVICES_ENABLE=event,trace,timestamp \
        ${EXE} ${ARGS} -P spot,perftest.experiment=trace
done

mv *.cali /g/g90/boehme3/experiments/caliper/annotation-perftest/

#!/bin/sh

export CALI_SERVICES_ENABLE=event:pthread:recorder:timestamp:statistics

for t in 2 4 8 12 16
do
    ./test/cali-throughput-pthread -t ${t}
done

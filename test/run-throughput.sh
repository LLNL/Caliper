#!/bin/sh

export CALI_CONFIG_FILE=./test/cali-throughput.config

for t in 2 4 8 12 16
do
    ./test/cali-throughput-pthread --val-attributes 8 --tree-attributes 8 -t ${t}
done

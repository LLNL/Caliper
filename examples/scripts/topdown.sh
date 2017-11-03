#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ -z "${TOPDOWN_GROUPBY}" ]; then
    >&2 echo "Need to set env variable TOPDOWN_GROUPBY."
    >&2 echo "Examples:"
    >&2 echo "TOPDOWN_GROUPBY=lulesh.phase"
    >&2 echo "TOPDOWN_GROUPBY=function,iteration"
    >&2 echo "TOPDOWN_GROUPBY=*"
    exit 1
fi  


ARCH_FAMILY=$(gcc -march=native -Q --help=target | grep march | awk -F' ' '{print $2}')
TOPDOWN_SCRIPT="${DIR}/../scripts/topdown.py"

if [ ! -f ${TOPDOWN_SCRIPT} ] 
then
    >&2 echo "No topdown python script available!"
    >&2 echo "Searched for $TOPDOWN_SCRIPT"
    >&2 echo "Aborting!"
    exit 1
fi

# Set up Caliper services
unset CALI_CONFIG_FILE

export CALI_LIBPFM_ENABLE_SAMPLING="false"
export CALI_LIBPFM_RECORD_COUNTERS="true"
export CALI_REPORT_FILENAME="topdown_counters.json"
export CALI_SERVICES_ENABLE="libpfm:pthread:event:trace:report:timestamp"

if [ "${ARCH_FAMILY}" == "ivybridge" ]
then
    LIBPFM_EVENTS=("BR_MISP_RETIRED.ALL_BRANCHES"
                   "CPU_CLK_UNHALTED.THREAD_P"
                   "CYCLE_ACTIVITY.CYCLES_NO_EXECUTE"
                   "CYCLE_ACTIVITY.STALLS_L1D_PENDING"
                   "CYCLE_ACTIVITY.STALLS_L2_PENDING"
                   "CYCLE_ACTIVITY.STALLS_LDM_PENDING"
                   "IDQ.MS_UOPS"
                   "IDQ_UOPS_NOT_DELIVERED.CORE"
                   "INT_MISC.RECOVERY_CYCLES"
                   "MACHINE_CLEARS.COUNT"
                   "MEM_LOAD_UOPS_RETIRED.L3_HIT"
                   "MEM_LOAD_UOPS_RETIRED.L3_MISS"
                   "RESOURCE_STALLS.SB"
                   "RS_EVENTS.EMPTY_CYCLES"
                   "UOPS_EXECUTED.THREAD"
                   "UOPS_EXECUTED.CORE_CYCLES_GE_1"
                   "UOPS_EXECUTED.CORE_CYCLES_GE_2"
                   "UOPS_ISSUED.ANY"
                   "UOPS_RETIRED.RETIRE_SLOTS")
else
    >&2 echo "Architecture \"${ARCH_FAMILY}\" not supported!"
    exit 1
fi

function join_by { 
    sep=$1
    shift
    echo -n "$1"
    while [[ "$#" -gt 1 ]]; do
        shift
        echo -n "$sep"
        echo -n "$1"
    done
}

export CALI_LIBPFM_EVENTS=$(join_by "," ${LIBPFM_EVENTS[@]})

SELECT_CLAUSE="*,sum(libpfm.counter."$(join_by "),sum(libpfm.counter." ${LIBPFM_EVENTS[@]})")"

export CALI_REPORT_CONFIG="SELECT ${SELECT_CLAUSE} GROUP BY ${TOPDOWN_GROUPBY} FORMAT json(quote-all)"

# Run the provided command
$@

# Run topdown analysis script
echo "Data collection complete, running topdown analysis..."
python ${TOPDOWN_SCRIPT} ${CALI_REPORT_FILENAME} ${ARCH_FAMILY}

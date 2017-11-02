#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

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
export CALI_REPORT_CONFIG="SELECT * FORMAT json(quote-all)"
export CALI_REPORT_FILENAME="topdown_counters.json"
export CALI_SERVICES_ENABLE="libpfm:pthread:event:trace:report:timestamp"

if [ "${ARCH_FAMILY}" == "ivybridge" ]
then
    CALI_LIBPFM_EVENTS="BR_MISP_RETIRED.ALL_BRANCHES"
    CALI_LIBPFM_EVENTS+=",CPU_CLK_UNHALTED.THREAD_P"
    CALI_LIBPFM_EVENTS+=",CYCLE_ACTIVITY.CYCLES_NO_EXECUTE"
    CALI_LIBPFM_EVENTS+=",CYCLE_ACTIVITY.STALLS_L1D_PENDING"
    CALI_LIBPFM_EVENTS+=",CYCLE_ACTIVITY.STALLS_L2_PENDING"
    CALI_LIBPFM_EVENTS+=",CYCLE_ACTIVITY.STALLS_LDM_PENDING"
    CALI_LIBPFM_EVENTS+=",IDQ.MS_UOPS"
    CALI_LIBPFM_EVENTS+=",IDQ_UOPS_NOT_DELIVERED.CORE"
    CALI_LIBPFM_EVENTS+=",INT_MISC.RECOVERY_CYCLES"
    CALI_LIBPFM_EVENTS+=",MACHINE_CLEARS.COUNT"
    CALI_LIBPFM_EVENTS+=",MEM_LOAD_UOPS_RETIRED.L3_HIT"
    CALI_LIBPFM_EVENTS+=",MEM_LOAD_UOPS_RETIRED.L3_MISS"
    CALI_LIBPFM_EVENTS+=",RESOURCE_STALLS.SB"
    CALI_LIBPFM_EVENTS+=",RS_EVENTS.EMPTY_CYCLES"
    CALI_LIBPFM_EVENTS+=",UOPS_EXECUTED.THREAD"
    CALI_LIBPFM_EVENTS+=",UOPS_EXECUTED.CORE_CYCLES_GE_1"
    CALI_LIBPFM_EVENTS+=",UOPS_EXECUTED.CORE_CYCLES_GE_2"
    CALI_LIBPFM_EVENTS+=",UOPS_ISSUED.ANY"
    CALI_LIBPFM_EVENTS+=",UOPS_RETIRED.RETIRE_SLOTS"
    
    export CALI_LIBPFM_EVENTS
else
    >&2 echo "Architecture \"${ARCH_FAMILY}\" not supported!"
    exit 1
fi

# Run the provided command
$@

# Run topdown analysis script
echo "Data collection complete, running topdown analysis..."
python ${TOPDOWN_SCRIPT} ${CALI_REPORT_FILENAME} ${ARCH_FAMILY}

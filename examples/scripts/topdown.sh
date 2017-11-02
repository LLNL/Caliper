#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

ARCH_FAMILY=$(gcc -march=native -Q --help=target | grep march | awk -F' ' '{print $2}')
CALI_CONFIG_FILE="${DIR}/../configs/topdown_${ARCH_FAMILY}.conf"

if [ ! -f ${CALI_CONFIG_FILE} ] 
then
    >&2 echo "No Caliper config file available for detected architecture: ${ARCH_FAMILY}"
    >&2 echo "Searched for $CALI_CONFIG_FILE"
    >&2 echo "Aborting!"
    exit 1
fi

echo "CALI_CONFIG_FILE=${CALI_CONFIG_FILE} $@"
CALI_CONFIG_FILE=${CALI_CONFIG_FILE} $@

CALI_REPORT_OUTPUT=$(awk -F'=' '$1 == "CALI_REPORT_FILENAME" {print $2}' ${CALI_CONFIG_FILE})

if [ ! -f ${CALI_REPORT_OUTPUT} ]
then
    >&2 echo "No output file produced! Set CALI_REPORT_FILENAME in ${CALI_CONFIG_FILE}"
    exit 1
fi

TOPDOWN_SCRIPT="${DIR}/../scripts/topdown.py"

if [ ! -f ${TOPDOWN_SCRIPT} ] 
then
    >&2 echo "No topdown python script available!"
    >&2 echo "Searched for $TOPDOWN_SCRIPT"
    >&2 echo "Aborting!"
    exit 1
fi

python ${TOPDOWN_SCRIPT} ${CALI_REPORT_OUTPUT} ${ARCH_FAMILY}

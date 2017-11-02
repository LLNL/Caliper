#!/bin/python
""" Script for deriving topdown metrics from a Json file produced by Caliper. """

from __future__ import print_function
import sys
import pandas as pd

pd.set_option('display.expand_frame_repr', False)

METRICS = [
    'frontend_bound',
    'bad_speculation',
    'retiring',
    'backend_bound',
    'frontend_latency_bound',
    'br_mispred_fraction',
    'memory_bound',
    'l1_bound',
    'l2_bound'
]


def eprint(*args, **kwargs):
    """ Print to stderr """
    print(*args, file=sys.stderr, **kwargs)


def derive_topdown_ivb(dfm):
    """ Perform topdown metric calculations for ivybridge architecture """

    dfm['clocks'] = dfm['libpfm.counter.CPU_CLK_UNHALTED.THREAD_P']
    dfm['slots'] = 4*dfm['clocks']
    dfm['frontend_bound'] = (dfm['libpfm.counter.IDQ_UOPS_NOT_DELIVERED.CORE']
                             / dfm['slots'])
    dfm['bad_speculation'] = ((dfm['libpfm.counter.UOPS_ISSUED.ANY']
                               - dfm['libpfm.counter.UOPS_RETIRED.RETIRE_SLOTS']
                               + 4*dfm['libpfm.counter.INT_MISC.RECOVERY_CYCLES'])
                              / dfm['slots'])
    dfm['retiring'] = (dfm['libpfm.counter.UOPS_RETIRED.RETIRE_SLOTS']
                       / dfm['slots'])
    dfm['backend_bound'] = (1 - (dfm['frontend_bound']
                                 + dfm['bad_speculation']
                                 + dfm['retiring']))
    dfm['frontend_latency_bound'] = (dfm['libpfm.counter.IDQ_UOPS_NOT_DELIVERED.CORE'].clip(lower=4)
                                     / dfm['clocks'])
    dfm['br_mispred_fraction'] = (dfm['libpfm.counter.BR_MISP_RETIRED.ALL_BRANCHES']
                                  / (dfm['libpfm.counter.BR_MISP_RETIRED.ALL_BRANCHES']
                                     + dfm['libpfm.counter.MACHINE_CLEARS.COUNT']))
    dfm['memory_bound'] = ((dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_LDM_PENDING']
                            + dfm['libpfm.counter.RESOURCE_STALLS.SB'])
                           / dfm['clocks'])
    dfm['l1_bound'] = ((dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_LDM_PENDING']
                        - dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_L1D_PENDING'])
                       / dfm['clocks'])
    dfm['l2_bound'] = ((dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_L1D_PENDING']
                        - dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_L2_PENDING'])
                       / dfm['clocks'])

    return dfm


def derive_topdown(dfm, arch):
    """ Determine topdown function to use, use it, then clean up the dataframe """

    if arch == 'sandybridge':
        eprint("Error, unsupported architecture " + arch)
        return dfm
    elif arch == 'ivybridge':
        dfm = derive_topdown_ivb(dfm)

    # Filter out entries where there were too few clock cycles to get an accurate metric
    dfm = dfm[dfm['clocks'] > 100]

    # Filter out entries where there was too little time to get an accurate metric
    dfm = dfm[dfm['time.inclusive.duration'] > 100]

    # Filter out entries where metrics are all NaN
    dfm = dfm.dropna(subset=METRICS, how='all')

    # Delete raw counts from output
    for column in dfm.columns:
        if 'libpfm.counter' in column:
            del dfm[column]

    return dfm


def main():
    """ Print all Caliper entries with their derived metrics """

    if len(sys.argv) != 3:
        eprint("Usage: " + sys.argv[0] + " <json file> <arch=sandybridge|ivybridge")

    dfm = pd.read_json(sys.argv[1])

    dfm = derive_topdown(dfm, sys.argv[2])

    # Format metrics as percentages
    percentage = '{:,.2%}'.format

    output = dfm.to_string(formatters={
        metric: percentage for metric in METRICS
    })

    print(output)


if __name__ == "__main__":
    main()

#!/bin/python
""" Script for deriving topdown metrics from a Json file produced by Caliper. """

from __future__ import print_function
import sys
import pandas as pd

pd.set_option('display.expand_frame_repr', False)

METRICS = [
    'retiring',
    'bad_speculation',
    'frontend_bound',
    'backend_bound',
    'branch_mispredict',
    'machine_clear',
    'frontend_latency',
    'frontend_bandwidth',
    'memory_bound',
    'core_bound',
    'mem_bound',
    'l1_bound',
    'l2_bound',
    'l3_bound',
    'uncore_bound',
]


def eprint(*args, **kwargs):
    """ Print to stderr """

    print(*args, file=sys.stderr, **kwargs)


def derive_topdown_ivb(dfm):
    """ Perform topdown metric calculations for ivybridge architecture """

    dfm['TEMPORARY_clocks'] = dfm['libpfm.counter.CPU_CLK_UNHALTED.THREAD_P']
    dfm['TEMPORARY_slots'] = 4*dfm['TEMPORARY_clocks']

    # Level 1 - Not Stalled
    dfm['retiring'] = (dfm['libpfm.counter.UOPS_RETIRED.RETIRE_SLOTS']
                       / dfm['TEMPORARY_slots'])
    dfm['bad_speculation'] = ((dfm['libpfm.counter.UOPS_ISSUED.ANY']
                               - dfm['libpfm.counter.UOPS_RETIRED.RETIRE_SLOTS']
                               + 4*dfm['libpfm.counter.INT_MISC.RECOVERY_CYCLES'])
                              / dfm['TEMPORARY_slots'])

    # Level 1 - Stalled
    dfm['frontend_bound'] = (dfm['libpfm.counter.IDQ_UOPS_NOT_DELIVERED.CORE']
                             / dfm['TEMPORARY_slots'])
    dfm['backend_bound'] = (1 - (dfm['frontend_bound']
                                 + dfm['bad_speculation']
                                 + dfm['retiring']))

    # Level 2 - Retiring
    # TODO: implement if possible

    # Level 2 - Bad speculation
    dfm['branch_mispredict'] = (dfm['libpfm.counter.BR_MISP_RETIRED.ALL_BRANCHES']
                                / (dfm['libpfm.counter.BR_MISP_RETIRED.ALL_BRANCHES']
                                   + dfm['libpfm.counter.MACHINE_CLEARS.COUNT']))
    dfm['machine_clear'] = (1 - dfm['branch_mispredict'])  # FIXME: is this correct?

    # Level 2 - Frontend Bound
    dfm['frontend_latency'] = (dfm['libpfm.counter.IDQ_UOPS_NOT_DELIVERED.CORE'].clip(lower=4)
                                     / dfm['TEMPORARY_clocks'])
    dfm['frontend_bandwidth'] = (1 - dfm['frontend_latency'])  # FIXME: is this correct?

    # Level 2 - Backend Bound
    dfm['memory_bound'] = (dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_LDM_PENDING']
                           / dfm['TEMPORARY_clocks'])
    dfm['TEMPORARY_be_bound_at_exe'] = ((dfm['libpfm.counter.CYCLE_ACTIVITY.CYCLES_NO_EXECUTE']
                                         + dfm['libpfm.counter.UOPS_EXECUTED.CORE_CYCLES_GE_1']
                                         - dfm['libpfm.counter.UOPS_EXECUTED.CORE_CYCLES_GE_2'])
                                        / dfm['TEMPORARY_clocks'])
    dfm['core_bound'] = (dfm['TEMPORARY_be_bound_at_exe']
                         - dfm['memory_bound'])

    # Level 3 - Memory bound
    dfm['TEMPORARY_l3_hit_fraction'] = (dfm['libpfm.counter.MEM_LOAD_UOPS_RETIRED.L3_HIT'] /
                                        (dfm['libpfm.counter.MEM_LOAD_UOPS_RETIRED.L3_HIT']
                                         + 7*dfm['libpfm.counter.MEM_LOAD_UOPS_RETIRED.L3_MISS']))
    dfm['TEMPORARY_l3_miss_fraction'] = (7*dfm['libpfm.counter.MEM_LOAD_UOPS_RETIRED.L3_MISS']
                                         / (dfm['libpfm.counter.MEM_LOAD_UOPS_RETIRED.L3_HIT']
                                            + 7*dfm['libpfm.counter.MEM_LOAD_UOPS_RETIRED.L3_MISS']))
    dfm['mem_bound'] = (dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_L2_PENDING']
                        * dfm['TEMPORARY_l3_miss_fraction']
                        / dfm['TEMPORARY_clocks'])
    dfm['l1_bound'] = ((dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_LDM_PENDING']
                        - dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_L1D_PENDING'])
                       / dfm['TEMPORARY_clocks'])
    dfm['l2_bound'] = ((dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_L1D_PENDING']
                        - dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_L2_PENDING'])
                       / dfm['TEMPORARY_clocks'])
    dfm['l3_bound'] = (dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_L2_PENDING']
                       * dfm['TEMPORARY_l3_hit_fraction']
                       / dfm['TEMPORARY_clocks'])
    dfm['uncore_bound'] = (dfm['libpfm.counter.CYCLE_ACTIVITY.STALLS_L2_PENDING']
                           / dfm['TEMPORARY_clocks'])

    for column in dfm.columns:
        if 'TEMPORARY' in column:
            del dfm[column]
        elif 'libpfm.counter' in column:
            del dfm[column]

    return dfm


def derive_topdown(dfm, arch):
    """ Determine topdown function to use, use it, then clean up the dataframe """

    if arch == 'ivybridge':
        dfm = derive_topdown_ivb(dfm)
    else:
        eprint("Error, unsupported architecture " + arch)
        return dfm

    return dfm


def max_column(row, columns):
    """ Returns the column name for the column with the largest value in row """

    return max([(column, row[column]) for column in columns],
               key=lambda t: t[1])[0]


def percentage_string(val):
    """ Returns a percentage-formatted string for a value, e.g. 0.9234 becomes 92.34% """

    return '{:,.2%}'.format(val)


def determine_boundedness(row):
    """ Determine the boundedness of a single row with topdown metrics """

    boundedness = []

    level_1 = max_column(row, ['retiring',
                               'bad_speculation',
                               'frontend_bound',
                               'backend_bound'])
    if str(row[level_1]) != 'nan' and str(row[level_1]) != 'inf':
        boundedness.append(level_1 + ' ' + percentage_string(row[level_1]))

    if level_1 == 'bad_speculation':
        level_2 = max_column(row, ['branch_mispredict',
                                   'machine_clear'])
        boundedness.append(level_2 + ' ' + percentage_string(row[level_2]))
    elif level_1 == 'frontend_bound':
        level_2 = max_column(row, ['frontend_latency',
                                   'frontend_bandwidth'])
        boundedness.append(level_2 + ' ' + percentage_string(row[level_2]))
    elif level_1 == 'backend_bound':
        level_2 = max_column(row, ['core_bound',
                                   'memory_bound'])
        boundedness.append(level_2 + ' ' + percentage_string(row[level_2]))
        if level_2 == 'memory_bound':
            level_3 = max_column(row, ['l1_bound',
                                       'l2_bound',
                                       'l3_bound',
                                       'mem_bound',
                                       'uncore_bound'])
            boundedness.append(level_3 + ' ' + percentage_string(row[level_3]))

    if len(boundedness) == 0:
        boundedness.append('undetermined')

    return boundedness


def analyze_topdown_metrics(dfm):
    """ Analyze topdown metrics to determine boundedness of different Caliper regions """

    dfm['boundedness'] = dfm.apply(determine_boundedness, axis=1)

    for metric in METRICS:
        del dfm[metric]

    return [dict([col for col in row.items() if str(col[1]) != 'nan'])
            for row in dfm.to_dict('index').values()]


def main():
    """ Print all Caliper entries with their derived metrics """

    if len(sys.argv) != 3:
        eprint("Usage: " + sys.argv[0] + " <json file> <arch, e.g. ivybridge>")

    dfm = pd.read_json(sys.argv[1])

    dfm = derive_topdown(dfm, sys.argv[2])

    analysis = analyze_topdown_metrics(dfm)

    for row in analysis:
        print(row)


if __name__ == "__main__":
    main()

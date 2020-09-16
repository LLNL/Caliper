# Copyright (c) 2020, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

#!/usr/bin/env python3

def _makelist(elem_or_list):
    if isinstance(elem_or_list, list):
        return elem_or_list
    else:
        return [ elem_or_list ]

def print_record(rec, ctx_key='path', metric_key='avg#inclusive#sum#time.duration'):
    if ctx_key in rec and metric_key in rec:
        print("  {0}: {1}s".format('/'.join(_makelist(rec[ctx_key])), rec[metric_key]))

#
# Use read_caliper_contents() to read the Caliper file
#

print("Using read_caliper_contents():")

from caliperreader.caliperreader import read_caliper_contents

records, globals = read_caliper_contents('example-profile.cali')

print("Globals:")
print("  globals['cali.caliper.version']: {0}".format(globals['cali.caliper.version']))
print("  globals['problem_size' {0}".format(globals['problem_size']))

print('\nMetrics:')
for rec in records:
    print_record(rec)


#
# Use a CaliperReader object to read the Caliper file
#

print ("\n\nUsing CaliperReader object:")

from caliperreader.caliperreader import CaliperReader

r = CaliperReader()

r.read('example-profile.cali')

print("Globals:")
print("  r.globals['cali.channel']: {0}".format(r.globals['cali.channel']))
print("  r.globals['figure_of_merit']: {0}".format(r.globals['figure_of_merit']))

print('\nMetrics:')
for rec in r.records:
    print_record(rec)

# We can query additional metadata about the Caliper attributes:
print("\nAttribute metadata:")
print("  r.attribute('function').is_nested(): {0}".format(r.attribute('function').is_nested()))
print("  r.attribute('function').attribute_type(): {0}".format(r.attribute('function').attribute_type()))
print("  r.attribute('figure_of_merit').get('adiak.type'): {0}".format(r.attribute('figure_of_merit').get('adiak.type')))

#
# Use CaliperStreamReader
#

print ("\n\nUsing CaliperStreamReader object:")

from caliperreader.caliperstreamreader import CaliperStreamReader

sr = CaliperStreamReader()

print('\nMetrics:')
sr.read('example-profile.cali', print_record)

# Basic smoke tests: aggregation

import unittest

import calipertest as calitest

class CaliperAggregationTest(unittest.TestCase):
    """ Caliper test case """

    def test_aggregate_default(self):
        target_cmd = [ './ci_test_aggregate' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate:event:recorder:timestamp',
            'CALI_TIMER_SNAPSHOT_DURATION' : 'true',
            'CALI_TIMER_INCLUSIVE_DURATION' : 'true',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, [ 'loop.id', 'function', 
                         'aggregate.sum#time.inclusive.duration',
                         'aggregate.min#time.inclusive.duration',
                         'aggregate.max#time.inclusive.duration',
                         'aggregate.sum#time.duration',
                         'aggregate.count' ] ))

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'event.end#function': 'foo', 
                'loop.id': 'A',
                'aggregate.count': 6 }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'event.end#function': 'foo', 
                'loop.id': 'B',
                'aggregate.count': 4 }))

    def test_aggregate_value_key(self):
        target_cmd = [ './ci_test_aggregate' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate:event:recorder:timestamp',
            'CALI_AGGREGATE_KEY'     : 'event.end#function:iteration',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'event.end#function': 'foo', 
                'iteration': 1,
                'aggregate.count': 3 }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'event.end#function': 'foo', 
                'iteration': 3,
                'aggregate.count': 1 }))

    def test_aggregate_attributes(self):
        target_cmd = [ './ci_test_aggregate' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate:event:recorder:timestamp',
            'CALI_TIMER_SNAPSHOT_DURATION' : 'true',
            'CALI_TIMER_INCLUSIVE_DURATION' : 'true',
            'CALI_AGGREGATE_ATTRIBUTES' : 'time.duration',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, [ 'loop.id', 'function',
                         'aggregate.sum#time.duration',
                         'aggregate.count' ] ))
        self.assertFalse(calitest.has_snapshot_with_keys(
            snapshots, [ 'aggregate.sum#time.inclusive.duration' ] ))

if __name__ == "__main__":
    unittest.main()

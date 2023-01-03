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
            snapshots, [ 'loop.id', 'region', 
                         'sum#time.inclusive.duration',
                         'min#time.inclusive.duration',
                         'max#time.inclusive.duration',
                         'sum#time.duration',
                         'count' ] ))

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'event.end#region': 'foo', 
                'loop.id': 'A',
                'count': '6' }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'event.end#region': 'foo', 
                'loop.id': 'B',
                'count': '4' }))

    def test_aggregate_combined_key(self):
        target_cmd = [ './ci_test_aggregate' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate:event:recorder',
            'CALI_AGGREGATE_KEY'     : 'event.end#region,iteration',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'event.end#region': 'foo', 
                'iteration': '1',
                'count': '3' }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'event.end#region': 'foo', 
                'iteration': '3',
                'count': '1' }))

    def test_aggregate_value_key(self):
        target_cmd = [ './ci_test_aggregate' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate:event:recorder',
            'CALI_AGGREGATE_KEY'     : 'iteration',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'iteration': '1',
                'count': '8' }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'iteration': '3',
                'count': '3' }))
        self.assertFalse(calitest.has_snapshot_with_keys(
            snapshots, [ 'region', 'loop.id' ]))

    def test_aggregate_nested(self):
        target_cmd = [ './ci_test_aggregate' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate:event:recorder',
            'CALI_AGGREGATE_KEY'     : 'path',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'loop.id'   : 'A',
                'region': 'foo',
                'count'     : '6' }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'loop.id'   : 'B',
                'region': 'foo',
                'count'     : '4' }))

    def test_aggregate_implicit_and_value(self):
        target_cmd = [ './ci_test_aggregate' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate:event:recorder',
            'CALI_AGGREGATE_KEY'     : '*,iteration',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'loop.id'    : 'A',
                'event.end#region': 'foo',
                'iteration'  : '1',
                'count'      : '2' }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'loop.id'    : 'B',
                'region'     : 'foo',
                'iteration'  : '3',
                'count'      : '1' }))

if __name__ == "__main__":
    unittest.main()

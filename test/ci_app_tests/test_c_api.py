# Tests of the C API

import unittest

import calipertest as calitest

class CaliperCAPITest(unittest.TestCase):
    """ Caliper C API test cases """

    def test_c_ann_trace(self):
        target_cmd = [ './ci_test_c_ann' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) >= 10)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, {'iteration', 'phase', 'time.inclusive.duration'}))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {'event.end#phase': 'initialization', 'phase': 'initialization'}))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {'event.end#iteration': 3, 'iteration': 3, 'phase': 'loop'}))

    def test_c_ann_snapshot(self):
        target_cmd = [ './ci_test_c_snapshot' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) >= 4)
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {'ci_test_c': 'snapshot', 'string_arg': 'teststring', 'int_arg': 42 }))

if __name__ == "__main__":
    unittest.main()

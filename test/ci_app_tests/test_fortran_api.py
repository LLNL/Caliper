# Tests of the Fortran API

import unittest

import calipertest as cat

class CaliperFortranAPITest(unittest.TestCase):
    """ Caliper Fortran API test cases """

    def test_f_ann_trace(self):
        target_cmd = [ './ci_test_f_ann', 'event-trace,output=stdout' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'annotation', 'time.inclusive.duration' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'annotation': 'main/work' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'annotation': 'main/foo' }))

    def test_f_ann_custom_spec(self):
        target_cmd = [ './ci_test_f_ann', 'custom-trace-spec,output=stdout' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'annotation': 'main/work' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'annotation': 'main/foo' }))

    def test_f_ann_globals(self):
        target_cmd = [ './ci_test_f_ann', 'event-trace,output=stdout' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '--list-globals', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'global.int.1': '42', 'global.int.2': '84' }))

if __name__ == "__main__":
    unittest.main()

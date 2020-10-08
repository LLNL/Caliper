# MemUsage tests

import unittest

import calipertest as calitest

class CaliperMemusageServiceTest(unittest.TestCase):
    """ Caliper MemUsage service test case """

    def test_memusage(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,memusage,trace,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'malloc.total.bytes',
                         'phase',
                         'iteration' }))

    def test_memusage_agg(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,memusage,aggregate,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'max#malloc.total.bytes',
                         'min#malloc.total.bytes',
                         'phase' }))

if __name__ == "__main__":
    unittest.main()

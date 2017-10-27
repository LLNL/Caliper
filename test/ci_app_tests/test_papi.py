# PAPI tests

import unittest

import calipertest as calitest

class CaliperPAPITest(unittest.TestCase):
    """ Caliper PAPI test case """

    def test_papi(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event:papi:trace:recorder',
            'CALI_PAPI_COUNTERS'     : 'PAPI_TOT_CYC',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'papi.PAPI_TOT_CYC', 'phase', 'iteration' }))

    def test_papi_aggr(self):
        # PAPI counters should be aggregated
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate:event:papi:recorder',
            'CALI_AGGREGATE_KEY'     : 'phase',
            'CALI_PAPI_COUNTERS'     : 'PAPI_TOT_CYC',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'sum#papi.PAPI_TOT_CYC', 
                         'min#papi.PAPI_TOT_CYC', 
                         'max#papi.PAPI_TOT_CYC',
                         'phase' }))

if __name__ == "__main__":
    unittest.main()

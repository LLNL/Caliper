# PAPI tests

import io
import unittest

import caliperreader
import calipertest as cat

class CaliperPAPITest(unittest.TestCase):
    """ Caliper PAPI test case """

    def test_papi(self):
        target_cmd = [ './ci_test_basic' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event:papi:trace:recorder',
            'CALI_PAPI_COUNTERS'     : 'PAPI_TOT_CYC',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'papi.PAPI_TOT_CYC', 'myphase', 'iteration' }))

    def test_papi_aggr(self):
        # PAPI counters should be aggregated
        target_cmd = [ './ci_test_basic' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate:event:papi:recorder',
            'CALI_AGGREGATE_KEY'     : 'myphase',
            'CALI_PAPI_COUNTERS'     : 'PAPI_TOT_CYC',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'sum#papi.PAPI_TOT_CYC', 
                         'min#papi.PAPI_TOT_CYC', 
                         'max#papi.PAPI_TOT_CYC',
                         'myphase' }))

if __name__ == "__main__":
    unittest.main()

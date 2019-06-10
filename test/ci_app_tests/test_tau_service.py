# report / C config test

import unittest

import calipertest as cat

class CaliperReportTest(unittest.TestCase):
    """ Caliper TAU service test case """

    def test_tau(self):
        target_cmd = [ './ci_test_mpi_before_cali' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'tau',
            'CALI_LOG_VERBOSITY'     : '0',
            'TAU_PROFILE'            : '0',
            'TAU_TRACE'              : '0'
        }

        #query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        query_output = cat.run_test(target_cmd, caliper_config)
        #snapshots = cat.get_snapshots_from_text(query_output)

        #self.assertTrue(len(snapshots) == 5)

        #self.assertTrue(cat.has_snapshot_with_attributes(
            #snapshots, { 'iteration#fooloop': '3', 'count': '1' }))

if __name__ == "__main__":
    unittest.main()

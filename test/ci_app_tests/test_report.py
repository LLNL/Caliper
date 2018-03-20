# report / C config test

import unittest

import calipertest as cat

class CaliperReportTest(unittest.TestCase):
    """ Caliper Report / Reader CSV export / C config API test case """

    def test_report_caliwriter(self):
        """ Test reader lib's CSV export via report service """
        
        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),iteration#fooloop WHERE loop=fooloop GROUP BY iteration#fooloop FORMAT expand' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'format cali',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) == 5)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'iteration#fooloop': '3', 'count': '1' }))


    def test_report(self):
        target_cmd = [ './ci_test_report' ]
        # create some distraction: read-from-env should be disabled
        env = { 'CALI_SERVICES_ENABLE' : 'debug' } 

        report_out = cat.run_test(target_cmd, env)
        
        lines = report_out.decode().splitlines()

        self.assertEqual(lines[0].rstrip(), 'function annotation count')
        self.assertEqual(lines[1].rstrip(), 'main     my phase       2')

if __name__ == "__main__":
    unittest.main()

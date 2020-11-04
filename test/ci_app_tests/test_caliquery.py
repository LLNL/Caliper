# Some tests for the cali-query tool

import json
import unittest

import calipertest as cat

class CaliperCaliQueryTest(unittest.TestCase):
    """ cali-query test cases """

    def test_caliquery_args(self):
        target_cmd = [ './ci_test_aggregate' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '--aggregate', 'count(),sum(time.inclusive.duration)', '--aggregate-key=loop.id', '-s', 'loop.id=A', '--json' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0',
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        self.assertEqual(obj[0]["path"], "A")
        self.assertEqual(obj[0]["count"], 19)
        self.assertTrue("sum#time.inclusive.duration" in obj[0])

    def test_caliquery_list_services(self):
        target_cmd = [ '../../src/tools/cali-query/cali-query', '--help=services' ]

        env = { 
            'CALI_LOG_VERBOSITY' : '0',
        }

        service_targets = [
            'aggregate', 'event', 'recorder', 'report', 'timestamp', 'trace'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        res = report_out.decode().split(',')

        for target in service_targets:
            if not target in res:
                self.fail('%s not found in log' % target)


if __name__ == "__main__":
    unittest.main()

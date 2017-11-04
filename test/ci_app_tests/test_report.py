# report / C config test

import unittest

import calipertest as cat

class CaliperReportTest(unittest.TestCase):
    """ Caliper Report / C API test case """

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

# report / C config test

import unittest

import calipertest as cat

class CaliperLogTest(unittest.TestCase):
    """ Caliper Log test case """

    def test_log_verbose(self):
        target_cmd = [ './ci_test_basic' ]

        env = { 'CALI_LOG_VERBOSITY' : '3', 
                'CALI_LOG_LOGFILE'   : 'stdout' 
        } 

        report_out = cat.run_test(target_cmd, env)
        
        lines = report_out.decode().splitlines()

        log_targets = [
            'CALI_LOG_VERBOSITY=3',
            '== CALIPER: Releasing channel default',
            '== CALIPER: Releasing Caliper thread data',
            'Blackboard',
            'Metadata tree',
            'Metadata memory pool'
        ]

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)


    def test_log_silent(self):
        target_cmd = [ './ci_test_basic' ]

        env = { 'CALI_LOG_VERBOSITY' : '0', 
                'CALI_LOG_LOGFILE'   : 'stdout' 
        } 

        report_out = cat.run_test(target_cmd, env)

        lines = report_out.decode().splitlines()

        self.assertTrue(len(lines) == 0)

if __name__ == "__main__":
    unittest.main()

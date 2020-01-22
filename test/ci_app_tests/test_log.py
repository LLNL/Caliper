# report / C config test

import unittest

import calipertest as cat


class CaliperLogTest(unittest.TestCase):
    """ Caliper Log test cases """

    def test_log_verbose(self):
        target_cmd = [ './ci_test_basic' ]

        env = { 'CALI_LOG_VERBOSITY' : '3',
                'CALI_LOG_LOGFILE'   : 'stdout'
        }

        log_targets = [
            'CALI_LOG_VERBOSITY=3',
            '== CALIPER: default: snapshot scopes: process thread',
            '== CALIPER: Releasing channel default',
            '== CALIPER: Releasing Caliper thread data',
            'Blackboard',
            'Metadata tree',
            'Metadata memory pool'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_scope_parse_error_msgs(self):
        target_cmd = [ './ci_test_basic' ]

        env = {
            'CALI_LOG_VERBOSITY' : '0',
            'CALI_LOG_LOGFILE'   : 'stdout',
            'CALI_CHANNEL_SNAPSHOT_SCOPES' : 'foo',
            'CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE' : 'bar'
        }

        log_targets = [
            'Invalid value "foo" for CALI_CHANNEL_SNAPSHOT_SCOPES',
            'Invalid value "bar" for CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

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

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        self.assertTrue(len(lines) == 0)

if __name__ == "__main__":
    unittest.main()

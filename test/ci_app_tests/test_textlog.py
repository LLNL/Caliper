# Tests for the textlog service

import unittest

import calipertest as cat

class CaliperTextlogTest(unittest.TestCase):
    """ Caliper textlog test cases """

    def test_textlog(self):
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'      : 'event,textlog',
            'CALI_TEXTLOG_TRIGGER'      : 'iteration#mainloop',
            'CALI_TEXTLOG_FORMATSTRING' : '%function% iteration: %[2]iteration#mainloop',
            'CALI_LOG_VERBOSITY'        : '0',
        }

        log_targets = [
            'main iteration:  0',
            'main iteration:  1',
            'main iteration:  2',
            'main iteration:  3'
        ]

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_textlog_autostring(self):
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'      : 'event,textlog,timestamp',
            'CALI_TEXTLOG_TRIGGER'      : 'loop',
            'CALI_LOG_VERBOSITY'        : '0',
        }

        log_targets = [
            'loop=mainloop/fooloop'
        ]

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

if __name__ == "__main__":
    unittest.main()

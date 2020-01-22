import unittest

import calipertest as cat


class CaliperValidatorTest(unittest.TestCase):
    """ Caliper validator service test cases """

    def test_validator_nesting_error(self):
        target_cmd = [ '../cali-test', 'nesting-error' ]

        env = {
            'CALI_LOG_VERBOSITY'   : '1',
            'CALI_LOG_LOGFILE'     : 'stdout',
            'CALI_SERVICES_ENABLE' : 'validator'
        }

        log_targets = [
            'validator: incorrect nesting: trying to end "test.nesting-error.a"="11" but current attribute is "test.nesting-error.b"',
            'validator: Annotation nesting errors found'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_validator_unclosed_region(self):
        target_cmd = [ '../cali-test', 'unclosed-region' ]

        env = {
            'CALI_LOG_VERBOSITY'   : '1',
            'CALI_LOG_LOGFILE'     : 'stdout',
            'CALI_SERVICES_ENABLE' : 'validator'
        }

        log_targets = [
            'validator: Regions not closed: test.unclosed_region=101',
            'validator: Annotation nesting errors found'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_validator_pass_blob(self):
        target_cmd = [ '../cali-test', 'blob' ]

        env = {
            'CALI_LOG_VERBOSITY'   : '1',
            'CALI_LOG_LOGFILE'     : 'stdout',
            'CALI_SERVICES_ENABLE' : 'validator'
        }

        log_targets = [
            'validator: No annotation nesting errors found'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_validator_pass_macros(self):
        target_cmd = [ './ci_test_macros', '0' ]

        env = {
            'CALI_LOG_VERBOSITY'   : '1',
            'CALI_LOG_LOGFILE'     : 'stdout',
            'CALI_SERVICES_ENABLE' : 'validator'
        }

        log_targets = [
            'validator: No annotation nesting errors found'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)


if __name__ == "__main__":
    unittest.main()

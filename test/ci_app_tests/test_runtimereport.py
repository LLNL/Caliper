# Runtime report tests

import json
import os
import unittest

import calipertest as cat

class CaliperRuntimeReportTest(unittest.TestCase):
    """ Runtime report controller """
    def test_runtime_report_default(self):
        target_cmd = [ './ci_test_macros', '10', 'runtime-report,output=stdout' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        log_targets = [
            'Path',
            'main',
            '  main loop',
            '    foo',
            '      fooloop'
        ]

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_runtime_report_nompi(self):
        target_cmd = [ './ci_test_macros', '10', 'runtime-report,aggregate_across_ranks=false,output=stdout' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        log_targets = [
            'Time (E) Time (I) Time % (E) Time % (I)',
            'main',
            '  main loop',
            '    foo',
            '      fooloop'
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

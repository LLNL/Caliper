# MPI application tests
# For simplicity, these tests only run on a single rank without mpiexec

import json
import os
import unittest

import calipertest as cat

class CaliperLoopReportTest(unittest.TestCase):
    """ Loop report controller (summary info) """
    def test_loopreport_summary(self):
        target_cmd = [ './ci_test_macros', '10', 'loop-report,summary=true,timeseries=false,output=stdout', '20' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        log_targets = [
            'Loop summary:',
            'Loop     Iterations Time (s)',
            'mainloop         20'
        ]

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    """ Loop report controller (summary info) """
    def test_loopreport_timeseries(self):
        target_cmd = [ './ci_test_macros', '0', 'loop-report,timeseries=true,summary=false,iteration_interval=15,output=stdout', '75' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        log_targets = [
            'Iteration summary (mainloop):',
            'Block Iterations Time (s)',
            '    0         15',
            '   30         15',
            '   60         15'
        ]

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    """ Loop report controller (timeseries info, display default number of rows (20)) """
    def test_loopreport_display_default_num_rows(self):
        target_cmd = [ './ci_test_macros', '0', 'loop-report,timeseries=true,summary=false,iteration_interval=2,output=stdout', '80' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        self.assertGreater(len(lines), 20)
        self.assertLess(len(lines), 28)

    """ Loop report controller (timeseries info, display all rows) """
    def test_loopreport_display_all_rows(self):
        target_cmd = [ './ci_test_macros', '0', 'loop-report,timeseries.rows=0,timeseries=true,summary=false,iteration_interval=2,output=stdout', '80' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        self.assertGreater(len(lines), 40)
        self.assertLess(len(lines), 48)

if __name__ == "__main__":
    unittest.main()

# Callpath sample report tests

import json
import os
import unittest

import calipertest as cat

class CaliperSampleReportTest(unittest.TestCase):
    """ Callpath sample report controller """
    def test_sample_report_nompi(self):
        target_cmd = [ './ci_test_macros', '5000', 'callpath-sample-report,output=stdout' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        log_targets = [
            'Path',
            'main',
            '  foo'
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
        target_cmd = [ './ci_test_macros', '5000', 'callpath-sample-report,aggregate_across_ranks=false,output=stdout' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        log_targets = [
            'Samples Time (sec)',
            'main',
            '  foo'
        ]

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_hatchet_callpath_sample_profile(self):
        target_cmd = [ './ci_test_macros', '5000', 'hatchet-sample-profile(use.mpi=false,output=stdout,output.format=cali)' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 0)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'loop', 'function', 'source.function#callpath.address' }))

if __name__ == "__main__":
    unittest.main()

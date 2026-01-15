# Callpath sample report tests

import io
import unittest

import caliperreader
import calipertest as cat

class CaliperSampleReportTest(unittest.TestCase):
    """ Callpath sample report controller """

    def test_sample_report_nompi(self):
        target_cmd = [ './ci_test_macros', '5000', 'sample-report,aggregate_across_ranks=false,output=stdout' ]

        log_targets = [
            'Samples Time (sec)',
            'main',
            '  main loop'
        ]

        report_out,_ = cat.run_test(target_cmd, None)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_sample_profile(self):
        target_cmd = [ './ci_test_macros', '5000', 'sample-profile(use.mpi=false,output=stdout)' ]

        out,_ = cat.run_test(target_cmd, None)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 0)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'loop', 'region', 'source.function#cali.sampler.pc' }))

if __name__ == "__main__":
    unittest.main()

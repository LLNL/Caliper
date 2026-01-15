# Callpath sample report tests

import io
import json
import unittest

import caliperreader
import calipertest as cat

class CaliperSamplingTest(unittest.TestCase):
    """ Test sampling, callpath, and symbollookup services """

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

    def test_sampler_symbollookup(self):
            target_cmd = [ './ci_test_macros', '5000' ]

            caliper_config = {
                'CALI_SERVICES_ENABLE'   : 'sampler:symbollookup:trace:recorder',
                'CALI_SYMBOLLOOKUP_LOOKUP_FILE'   : 'true',
                'CALI_SYMBOLLOOKUP_LOOKUP_LINE'   : 'true',
                'CALI_SYMBOLLOOKUP_LOOKUP_MODULE' : 'true',
                'CALI_RECORDER_FILENAME' : 'stdout',
            }

            out,_ = cat.run_test(target_cmd, caliper_config)
            snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

            self.assertTrue(len(snapshots) > 1)

            self.assertTrue(cat.has_snapshot_with_keys(
                snapshots, { 'cali.sampler.pc',
                            'source.function#cali.sampler.pc',
                            'source.file#cali.sampler.pc',
                            'source.line#cali.sampler.pc',
                            'sourceloc#cali.sampler.pc',
                            'module#cali.sampler.pc',
                            'region', 'loop' }))

    def test_sample_profile_lookup(self):
        target_cmd = [ './ci_test_macros', '5000', 'sample-profile(use.mpi=false,output.format=json-split,output=stdout,callpath=false,source.location=true,source.module=true,source.function=true)' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY' : '0'
        }

        obj = json.loads( cat.run_test(target_cmd, caliper_config)[0] )

        self.assertEqual(set(obj['columns']), { 'count', 'time', 'Function', 'Source', 'Module', 'path' } )

    def test_callpath_service(self):
        target_cmd = [ './ci_test_alloc' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'callpath,trace,recorder',
            'CALI_CALLPATH_USE_NAME' : 'true',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) == 3)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'callpath.address', 'callpath.regname', 'test_alloc.allocated.0', 'region' }))

        sreg = cat.get_snapshot_with_keys(snapshots, { 'callpath.regname', 'test_alloc.allocated.0' })

        self.assertTrue('main' in sreg.get('callpath.regname'))

if __name__ == "__main__":
    unittest.main()

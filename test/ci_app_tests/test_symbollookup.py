# Sampler/symbollookup tests

import io
import json
import unittest

import caliperreader
import calipertest as cat

class CaliperSamplerTest(unittest.TestCase):
    """ Caliper sampler test case """

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

if __name__ == "__main__":
    unittest.main()

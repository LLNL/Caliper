# Sampler/symbollookup tests

import json
import unittest

import calipertest as cat

class CaliperSamplerTest(unittest.TestCase):
    """ Caliper sampler test case """

    def test_sampler_symbollookup(self):
        target_cmd = [ './ci_test_macros', '5000' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'sampler:symbollookup:trace:recorder',
            'CALI_SYMBOLLOOKUP_LOOKUP_FILE'   : 'true',
            'CALI_SYMBOLLOOKUP_LOOKUP_LINE'   : 'true',
            'CALI_SYMBOLLOOKUP_LOOKUP_MODULE' : 'true',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'cali.sampler.pc', 
                         'source.function#cali.sampler.pc', 
                         'source.file#cali.sampler.pc',
                         'source.line#cali.sampler.pc',
                         'sourceloc#cali.sampler.pc',
                         'module#cali.sampler.pc',
                         'function', 'loop' }))

    def test_hatchet_sample_profile_lookup(self):
        target_cmd = [ './ci_test_macros', '5000', 'hatchet-sample-profile(use.mpi=false,output=stdout,sample.callpath=false,lookup.sourceloc=true,lookup.module=true)' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY' : '0'
        }

        obj = json.loads( cat.run_test(target_cmd, caliper_config)[0] )

        self.assertEqual(set(obj['columns']), { 'count', 'time', 'sourceloc#cali.sampler.pc', 'module#cali.sampler.pc', 'path' } )

if __name__ == "__main__":
    unittest.main()

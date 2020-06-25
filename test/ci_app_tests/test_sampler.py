# Sampler/symbollookup tests

import unittest

import calipertest as cat

class CaliperSamplerTest(unittest.TestCase):
    """ Caliper sampler test case """

    def test_hatchet_sample_profile(self):
        target_cmd = [ './ci_test_macros', '5000', 'hatchet-sample-profile(use.mpi=false,output=stdout,output.format=cali)' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 0)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'loop', 'function' }))

if __name__ == "__main__":
    unittest.main()

# Sampler/symbollookup tests

import unittest

import calipertest as cat

class CaliperSamplerTest(unittest.TestCase):
    """ Caliper sampler test case """

    def test_sampler_symbollookup(self):
        target_cmd = [ './ci_dgemm_memtrack' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'sampler:symbollookup:trace:recorder',
            'CALI_SYMBOLLOOKUP_LOOKUP_FILE' : 'true',
            'CALI_SYMBOLLOOKUP_LOOKUP_LINE' : 'true',
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
                         'function' }))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'source.function#cali.sampler.pc' : 'ci_dgemm_do_work' }))

if __name__ == "__main__":
    unittest.main()

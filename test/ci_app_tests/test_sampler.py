# Sampler/symbollookup tests

import unittest

import calipertest as calitest

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

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'cali.sampler.pc', 
                         'source.function#cali.sampler.pc', 
                         'source.file#cali.sampler.pc',
                         'source.line#cali.sampler.pc',
                         'sourceloc#cali.sampler.pc',
                         'function' }))

        sfile = calitest.get_snapshot_with_keys(snapshots, { 'source.file#cali.sampler.pc' })

        self.assertTrue('ci_dgemm_memtrack' in sfile.get('source.file#cali.sampler.pc'))

if __name__ == "__main__":
    unittest.main()

# callpath service tests

import unittest

import calipertest as cat

class CaliperCallpathTest(unittest.TestCase):
    """ Caliper callpath smoketest """

    def test_callpath(self):
        target_cmd = [ './ci_test_alloc' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'callpath,trace,recorder',
            'CALI_CALLPATH_USE_NAME' : 'true',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) == 3)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'callpath.address', 'callpath.regname', 'test_alloc.allocated.0', 'function' }))

        sreg = cat.get_snapshot_with_keys(snapshots, { 'callpath.regname', 'test_alloc.allocated.0' })

        self.assertTrue('main' in sreg.get('callpath.regname'))

if __name__ == "__main__":
    unittest.main()

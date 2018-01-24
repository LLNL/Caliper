# Tests of the alloc service

import unittest

import calipertest as cat

class CaliperAllocServiceTest(unittest.TestCase):
    """ Caliper sysalloc service test cases """

    def helper_test_hook(self, hook):

        # test allocated.0

        self.assertTrue(cat.has_snapshot_with_keys(
            self.snapshots, { 'test_alloc.' + hook + '_hook',
                              'test_alloc.allocated.0',
                              'alloc.uid#ptr_in',
                              'ptr_in', 'ptr_out' }))

        self.assertTrue(cat.has_snapshot_with_attributes(
            self.snapshots, {'test_alloc.' + hook + '_hook' : 'true',
                             'test_alloc.allocated.0' : 'true',
                             'alloc.index#ptr_in'     : '0'
                             }))

        self.assertFalse(cat.has_snapshot_with_keys(
            self.snapshots, { 'test_alloc.' + hook + '_hook',
                             'test_alloc.allocated.0',
                             'alloc.uid#ptr_out' }))

        # test allocated.1

        self.assertTrue(cat.has_snapshot_with_keys(
            self.snapshots, { 'test_alloc.' + hook + '_hook',
                             'test_alloc.allocated.1',
                             'alloc.uid#ptr_in',
                             'ptr_in', 'ptr_out' }))

        index = '41' if hook == 'calloc' else '167'  # 41*sizeof(int)-1
        self.assertTrue(cat.has_snapshot_with_attributes(
            self.snapshots, {'test_alloc.' + hook + '_hook' : 'true',
                             'test_alloc.allocated.1' : 'true',
                             'alloc.index#ptr_in'     : index
                             }))

        self.assertFalse(cat.has_snapshot_with_keys(
            self.snapshots, { 'test_alloc.' + hook + '_hook',
                             'test_alloc.allocated.1',
                             'alloc.uid#ptr_out' }))

        # test allocated.freed

        self.assertTrue(cat.has_snapshot_with_keys(
            self.snapshots, { 'test_alloc.' + hook + '_hook',
                             'test_alloc.freed',
                             'ptr_in', 'ptr_out' }))

        self.assertFalse(cat.has_snapshot_with_keys(
            self.snapshots, { 'test_alloc.' + hook + '_hook',
                             'test_alloc.freed',
                             'alloc.uid#ptr_in' }))


    def test_alloc_hooks(self):
        target_cmd = [ './ci_test_alloc_hooks' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_ALLOC_RESOLVE_ADDRESSES' : 'true',
            'CALI_SERVICES_ENABLE'         : 'alloc:recorder:sysalloc:trace',
            'CALI_RECORDER_FILENAME'       : 'stdout',
            'CALI_LOG_VERBOSITY'           : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        self.snapshots = cat.get_snapshots_from_text(query_output)

        self.helper_test_hook('malloc')
        self.helper_test_hook('calloc')
        self.helper_test_hook('realloc')


if __name__ == "__main__":
    unittest.main()

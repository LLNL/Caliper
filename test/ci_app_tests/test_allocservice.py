# Tests of the alloc service

import unittest

import calipertest as cat

class CaliperAllocServiceTest(unittest.TestCase):
    """ Caliper alloc service test cases """

    def test_alloc(self):
        target_cmd = [ './ci_test_alloc' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'        : 'alloc:recorder:trace:event',
            'CALI_EVENT_ENABLE_SNAPSHOT_INFO' : 'false',
            'CALI_ALLOC_RESOLVE_ADDRESSES': 'true',
            'CALI_RECORDER_FILENAME'      : 'stdout',
            'CALI_LOG_VERBOSITY'          : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        # test allocated.0

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'mem.alloc', 'alloc.uid', 'alloc.address', 'alloc.total_size' }))
        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'mem.free', 'alloc.uid', 'alloc.address', 'alloc.total_size' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'mem.alloc': 'test_alloc_A' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'mem.free': 'test_alloc_A'  }))

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'test_alloc.allocated.0', 'ptr_in', 'ptr_out' }))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'test_alloc.allocated.0' : 'true',
                        'alloc.uid#ptr_in'       : '1',
                        'alloc.index#ptr_in'     : '0',
                        'alloc.label#ptr_in'     : 'test_alloc_A'
                    }))

        self.assertFalse(cat.has_snapshot_with_keys(
            snapshots, { 'test_alloc.allocated.0', 'alloc.uid#ptr_out' }))

        # test allocated.1

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'test_alloc.allocated.1', 'ptr_in', 'ptr_out' }))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'test_alloc.allocated.1' : 'true',
                        'alloc.uid#ptr_in'       : '1',
                        'alloc.index#ptr_in'     : '41',
                        'alloc.label#ptr_in'     : 'test_alloc_A'
                    }))

        self.assertFalse(cat.has_snapshot_with_keys(
            snapshots, { 'test_alloc.allocated.1', 'alloc.uid#ptr_out' }))

        # test allocated.freed

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'test_alloc.freed', 'ptr_in', 'ptr_out' }))

        self.assertFalse(cat.has_snapshot_with_keys(
            snapshots, { 'test_alloc.freed', 'alloc.uid#ptr_in' }))


if __name__ == "__main__":
    unittest.main()

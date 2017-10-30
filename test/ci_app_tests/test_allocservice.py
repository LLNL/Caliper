# Tests of the alloc service

import unittest

import calipertest as calitest

class CaliperAllocServiceTest(unittest.TestCase):
    """ Caliper alloc service test cases """

    def test_alloc(self):
        target_cmd = [ './ci_test_alloc' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'alloc:recorder:trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) == 4)

        # test allocated.0

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'test_alloc.allocated.0', 'ptr_in', 'ptr_out' }))

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {'test_alloc.allocated.0' : 'true', 
                        'alloc.label#ptr_in'     : 'test_alloc_A',
                        'alloc.index#ptr_in'     : '0'
                    }))

        self.assertFalse(calitest.has_snapshot_with_keys(
            snapshots, { 'test_alloc.allocated.0', 'alloc.label#ptr_out' }))

        # test allocated.1

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'test_alloc.allocated.1', 'ptr_in', 'ptr_out' }))

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {'test_alloc.allocated.1' : 'true', 
                        'alloc.label#ptr_in'     : 'test_alloc_A',
                        'alloc.index#ptr_in'     : '41'
                    }))

        self.assertFalse(calitest.has_snapshot_with_keys(
            snapshots, { 'test_alloc.allocated.1', 'alloc.label#ptr_out' }))

        # test allocated.freed

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'test_alloc.freed', 'ptr_in', 'ptr_out' }))

        self.assertFalse(calitest.has_snapshot_with_keys(
            snapshots, { 'test_alloc.freed', 'alloc.label#ptr_in' }))

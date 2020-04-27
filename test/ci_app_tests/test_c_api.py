# Tests of the C API

import unittest

import calipertest as cat

class CaliperCAPITest(unittest.TestCase):
    """ Caliper C API test cases """

    def test_c_ann_trace(self):
        target_cmd = [ './ci_test_c_ann', 'event-trace,output=stdout' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) >= 10)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, {'iteration', 'phase', 'time.inclusive.duration', 'global.int' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'event.end#phase': 'loop', 'phase': 'loop'}))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'event.end#iteration': '3', 'iteration': '3', 'phase': 'loop'}))
        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'attr.int', 'attr.dbl', 'attr.str', 'ci_test_c_ann.setbyname' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'attr.int' : '20', 'attr.str' : 'fidibus' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'test-attr-with-metadata' : 'abracadabra' }))

    def test_c_ann_globals(self):
        target_cmd = [ './ci_test_c_ann' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e', '--list-globals' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) == 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'global.double',
                         'global.string',
                         'global.int',
                         'global.uint',
                         'cali.caliper.version'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'global.int'    : '1337',
                         'global.string' : 'my global string',
                         'global.uint'   : '42'
            }))

    def test_c_ann_metadata(self):
        target_cmd = [ './ci_test_c_ann' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e', '--list-attributes',
                       '--print-attributes', 'cali.attribute.name,cali.attribute.type,meta-attr' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name' : 'meta-attr',
                         'cali.attribute.type' : 'int' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name' : 'test-attr-with-metadata',
                         'cali.attribute.type' : 'string',
                         'meta-attr'           : '47' }))

    def test_c_ann_snapshot(self):
        target_cmd = [ './ci_test_c_snapshot' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) >= 4)
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'ci_test_c': 'snapshot', 'string_arg': 'teststring', 'int_arg': '42' }))


if __name__ == "__main__":
    unittest.main()

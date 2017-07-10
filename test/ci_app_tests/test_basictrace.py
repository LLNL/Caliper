# Basic smoke tests: create and read a simple trace 

import unittest

import calipertest as calitest

class CaliperBasicTraceTest(unittest.TestCase):
    """ Caliper test case """

    def test_basic_trace(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 10)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, {'iteration', 'phase', 'time.inclusive.duration'}))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {'event.end#phase': 'initialization', 'phase': 'initialization'}))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {'event.end#iteration': '3', 'iteration': '3', 'phase': 'loop'}))

    def test_macros(self):
        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 10)

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'function'   : 'main',
                'loop'       : 'mainloop',
                'iteration#mainloop' : '3' }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'function'   : 'main/foo',
                'annotation' : 'pre-loop',
                'statement'  : 'foo.init' }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'function'   : 'main/foo',
                'loop'       : 'mainloop/fooloop',
                'iteration#fooloop' : '3' }))
        
    def test_property_override(self):
        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e', '--list-attributes' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0',
            'CALI_CALIPER_ATTRIBUTE_PROPERTIES' : 'annotation=process_scope'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'cali.attribute.name' : 'function',
                'cali.attribute.prop' : '276',
                'cali.attribute.type' : 'string' }))

        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {
                'cali.attribute.name' : 'annotation',
                'cali.attribute.prop' : '12',
                'cali.attribute.type' : 'string' }))

if __name__ == "__main__":
    unittest.main()

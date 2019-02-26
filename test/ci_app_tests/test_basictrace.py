# Basic smoke tests: create and read a simple trace 

import json
import unittest

import calipertest as cat

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

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 10)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, {'iteration', 'phase', 'time.inclusive.duration'}))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'event.end#phase': 'initialization', 'phase': 'initialization'}))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'event.end#iteration': '3', 'iteration': '3', 'phase': 'loop'}))

    def test_ann_metadata(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '--list-attributes', '-e', '--print-attributes', 'cali.attribute.name,meta.int' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name': 'phase',
                         'meta.int':            '42',
            }))

    def test_largetrace(self):
        target_cmd = [ './ci_test_loop', '80000' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            # 'CALI_EVENT_ENABLE_SNAPSHOT_INFO' : 'false',
            'CALI_REPORT_FILENAME'   : 'stdout',
            'CALI_REPORT_CONFIG'     : 'select function,count() group by function format cali',
            'CALI_TRACE_BUFFER_SIZE' : '1',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main/foo', 'count' : '80000'}))
        

    def test_globals(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e', '--list-globals' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertEqual(len(snapshots), 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'cali.caliper.version' } ) )
        
    def test_esc(self):
        target_cmd = [ './ci_test_esc' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-j' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        self.assertEqual(obj[0]['event.set# =\\weird ""attribute"=  '], '  \\\\ weird," name",' )
        

    def test_macros(self):
        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 10)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'function'   : 'main',
                'loop'       : 'mainloop',
                'iteration#mainloop' : '3' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'function'   : 'main/foo',
                'annotation' : 'pre-loop',
                'statement'  : 'foo.init' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
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

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'cali.attribute.name' : 'function',
                'cali.attribute.prop' : '276',
                'cali.attribute.type' : 'string' }))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'cali.attribute.name' : 'annotation',
                'cali.attribute.prop' : '12',
                'cali.attribute.type' : 'string' }))

    def test_postprocess_snapshot(self):
        target_cmd = [ './ci_test_postprocess_snapshot' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'trace:recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0',
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'snapshot.val'     : '49',
                'postprocess.val'  : '42',
                'postprocess.node' : '36' }))

    def test_binding(self):
        target_cmd = [ './ci_test_binding' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0',
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'testbinding' : 'binding.nested=outer/binding.nested=inner' }))


if __name__ == "__main__":
    unittest.main()

# Basic smoke tests: create and read a simple trace, test various options

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

    def test_cali_config(self):
        # Test the builtin ConfigManager (CALI_CONFIG env var)

        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0',
            'CALI_CONFIG'            : 'event-trace,output=stdout'
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
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '--list-attributes', '-e', '--print-attributes', 'cali.attribute.name,meta.int,cali.attribute.prop' ]

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
                         'cali.attribute.prop': '20' # default property: CALI_ATTR_SCOPE_THREAD
            }))

    def test_default_scope_switch(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '--list-attributes', '-e', '--print-attributes', 'cali.attribute.name,meta.int,cali.attribute.prop' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE' : 'process',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name': 'phase',
                         'meta.int':            '42',
                         'cali.attribute.prop': '12' # CALI_ATTR_SCOPE_PROCESS
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name': 'iteration',
                         'cali.attribute.prop': '13' # CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE
            }))

    def test_sec_unit_selection(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '--list-attributes', '-e', '--print-attributes', 'cali.attribute.name,time.unit' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_TIMER_SNAPSHOT_DURATION' : 'true',
            'CALI_TIMER_UNIT'        : 'sec',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0',
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name': 'time.duration',
                         'time.unit':           'sec'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name': 'time.inclusive.duration',
                         'time.unit':           'sec'
            }))

    def test_usec_unit_selection(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '--list-attributes', '-e', '--print-attributes', 'cali.attribute.name,time.unit' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_TIMER_SNAPSHOT_DURATION' : 'true',
            'CALI_TIMER_UNIT'        : 'usec',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0',
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name': 'time.duration',
                         'time.unit':           'usec'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name': 'time.inclusive.duration',
                         'time.unit':           'usec'
            }))

    def test_largetrace(self):
        target_cmd = [ './ci_test_macros', '0', 'none', '400' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            # 'CALI_EVENT_ENABLE_SNAPSHOT_INFO' : 'false',
            'CALI_REPORT_FILENAME'   : 'stdout',
            'CALI_REPORT_CONFIG'     : 'select function,event.end#loop,count() group by function,event.end#loop format cali',
            'CALI_TRACE_BUFFER_SIZE' : '1',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main/foo', 'event.end#loop': 'fooloop', 'count' : '400' }))

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

    def test_globals_selection(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-G', '-q', 'select cali.caliper.version format expand' ]

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
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-j', '-s', 'cali.event.set' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        self.assertEqual(obj[0]['event.set# =\\weird ""attribute"=  '], '  \\\\ weird," name",' )

    def test_macros(self):
        # Use ConfigManager API here
        target_cmd = [ './ci_test_macros', '0', 'event-trace,output=stdout' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 10)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'function'     : 'main',
                'loop'         : 'main loop',
                'iteration#main loop' : '3',
                'region.count' : '1' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'function'   : 'main/foo',
                'annotation' : 'pre-loop',
                'statement'  : 'foo.init' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'function'   : 'main',
                'annotation' : 'before_loop' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'function'   : 'main/foo',
                'loop'       : 'main loop/fooloop',
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

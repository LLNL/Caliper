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
            snapshots, {'iteration', 'myphase', 'time.inclusive.duration.ns'}))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'event.end#myphase': 'initialization', 'myphase': 'initialization'}))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'event.end#iteration': '3', 'iteration': '3', 'myphase': 'loop'}))

    def test_cali_config(self):
        # Test the builtin ConfigManager (CALI_CONFIG env var)

        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0',
            'CALI_CONFIG'            : 'event-trace,time.inclusive,output=stdout'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 10)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, {'iteration', 'myphase', 'time.inclusive.duration.ns'}))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'event.end#myphase': 'initialization', 'myphase': 'initialization'}))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'event.end#iteration': '3', 'iteration': '3', 'myphase': 'loop'}))

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
            snapshots, { 'cali.attribute.name': 'myphase',
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
            snapshots, { 'cali.attribute.name': 'myphase',
                         'meta.int':            '42',
                         'cali.attribute.prop': '12' # CALI_ATTR_SCOPE_PROCESS
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'cali.attribute.name': 'iteration',
                         'cali.attribute.prop': '13' # CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_ASVALUE
            }))

    def test_largetrace(self):
        target_cmd = [ './ci_test_macros', '0', 'none', '400' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            # 'CALI_EVENT_ENABLE_SNAPSHOT_INFO' : 'false',
            'CALI_REPORT_FILENAME'   : 'stdout',
            'CALI_REPORT_CONFIG'     : 'select region,event.end#loop,count() group by region,event.end#loop format cali',
            'CALI_TRACE_BUFFER_SIZE' : '1',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : 'main/foo', 'event.end#loop': 'fooloop', 'count' : '400' }))

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
        target_cmd = [ './ci_test_basic', 'newline' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-j', '-s', 'cali.event.set' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        self.assertEqual(len(obj), 2)

        self.assertEqual(obj[0]['event.set# =\\weird ""attribute"=  '], '  \\\\ weird," name",' )
        self.assertEqual(obj[1]['event.set#newline'], 'A newline:\n!')

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
                'region'   : 'main',
                'loop'         : 'main loop',
                'iteration#main loop' : '3',
                'region.count' : '1' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : 'main/foo/pre-loop/foo.init' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : 'main/before_loop' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main/foo',
                'loop'       : 'main loop/fooloop',
                'iteration#fooloop' : '3' }))

    def test_property_override(self):
        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e', '--list-attributes' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0',
            'CALI_CALIPER_ATTRIBUTE_PROPERTIES' : 'region=process_scope'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'cali.attribute.name' : 'loop',
                'cali.attribute.prop' : '276',
                'cali.attribute.type' : 'string' }))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'cali.attribute.name' : 'region',
                'cali.attribute.prop' : '12',
                'cali.attribute.type' : 'string' }))

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

    def test_inclusive_region_filter(self):
        target_cmd = [ './ci_test_macros', '0', 'event-trace,include_regions="startswith(main)",output=stdout' ]
        query_cmd = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY' : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main',
                'loop'   : 'main loop'
            }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, {
                'region': 'pre-loop'
            }))

    def test_exclusive_region_filter(self):
        target_cmd = [ './ci_test_macros', '0', 'hatchet-region-profile,exclude_regions=before_loop,output.format=cali,output=stdout' ]
        query_cmd = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY' : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main/foo',
                'loop'   : 'main loop/fooloop' }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main/before_loop' }))

    def test_region_level_filter(self):
        target_cmd = [ './ci_test_macros', '0', 'hatchet-region-profile,level=phase,output=stdout' ]
        query_cmd = [ '../../src/tools/cali-query/cali-query', '-e' ]

        query_output = cat.run_test_with_query(target_cmd, query_cmd, None)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main/foo',
                'loop'   : 'main loop/fooloop' }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main/bar',
                'phase'  : 'after_loop' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main',
                'phase'  : 'after_loop' }))

    def test_branch_filter(self):
        target_cmd = [ './ci_test_macros', '0', 'hatchet-region-profile,include_branches=after_loop,output=stdout' ]
        query_cmd = [ '../../src/tools/cali-query/cali-query', '-e' ]

        query_output = cat.run_test_with_query(target_cmd, query_cmd, None)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main/foo',
                'loop'   : 'main loop/fooloop' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main/bar',
                'phase'  : 'after_loop' }))

if __name__ == "__main__":
    unittest.main()

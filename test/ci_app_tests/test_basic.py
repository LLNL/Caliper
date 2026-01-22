# Basic smoke tests: create and read a simple trace, test various options

import io
import json
import os
import time
import unittest

import caliperreader
import calipertest as cat

class CaliperBasicTest(unittest.TestCase):
    """ Test Caliper core functionality """

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
        """ Test the CALI_CONFIG environment variable """

        target_cmd = [ './ci_test_basic' ]

        out,_ = cat.run_test(target_cmd, { 'CALI_CONFIG': 'event-trace,time.inclusive,output=stdout' })
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

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

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_FILENAME'   : 'stdout',
            'CALI_REPORT_CONFIG'     : 'select region,event.end#loop,count() group by region,event.end#loop format cali',
            'CALI_TRACE_BUFFER_SIZE' : '1',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : [ 'main', 'foo' ], 'event.end#loop': 'fooloop', 'count' : '400' }))

    def test_thread(self):
        """ Test thread blackboard management """

        target_cmd = [ './ci_test_thread' ]

        out,_ = cat.run_test(target_cmd, { 'CALI_CONFIG': 'event-trace,output=stdout' })
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) >= 16)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, {'local', 'global', 'region'}))
        self.assertFalse(cat.has_snapshot_with_keys(
            snapshots, {'local', 'my_thread_id' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'my_thread_id' : '16',
                        'region'       : 'thread_proc',
                        'global'       : '999' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'my_thread_id' : '49',
                        'region'       : 'thread_proc',
                        'global'       : '999' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region'      : 'main',
                         'local'       : '99' }))

    def test_createdir(self):
        """ Test directory creation """
        target_cmd = [ './ci_test_macros', '0', 'runtime-profile,use.mpi=false,output.format=json,output=foo/bar/test.json' ]

        if os.path.exists('foo/bar/test.json'):
            os.remove('foo/bar/test.json')
        if os.path.exists('foo/bar'):
            os.removedirs('foo/bar')

        self.assertFalse(os.path.exists('foo/bar'))

        cat.run_test(target_cmd, {})

        self.assertTrue(os.path.isdir('foo/bar'))
        self.assertTrue(os.path.isfile('foo/bar/test.json'))

        if os.path.exists('foo/bar/test.json'):
            os.remove('foo/bar/test.json')
        if os.path.exists('foo/bar'):
            os.removedirs('foo/bar')

    def test_globals(self):
        """ Test Caliper built-in global variables and cali-query --list-globals option """

        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e', '--list-globals' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        t_begin = time.time()
        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        t_end = time.time()

        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertEqual(len(snapshots), 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'cali.caliper.version', 'starttime.sec', 'starttime.nsec' } ) )

        t_cali = int(snapshots[0]['starttime.sec'])

        self.assertLessEqual(int(t_begin), t_cali)
        self.assertLessEqual(t_cali, int(t_end))

    def test_globals_selection(self):
        """ Test cali-query selection option when used with --list-globals """

        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-G', '-q', 'select cali.caliper.version format expand' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertEqual(len(snapshots), 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'cali.caliper.version' } ) )

    def test_configmanager_metadata_import(self):
        """ Test metadata() keyword in config strings """

        target_cmd = [ './ci_test_macros', '0', 'runtime-profile(metadata(bla=garbl),output.format=json-split),metadata(file=example_node_info.json,keys=\"host.os,host.name\"),output=stdout' ]

        obj = json.loads( cat.run_test(target_cmd, {})[0] )

        self.assertEqual(obj['host.os.name'], 'TestOS')
        self.assertEqual(obj['host.os.version'], '3.11')
        self.assertEqual(obj['host.name'], 'test42')
        self.assertEqual(obj['bla'], 'garbl')
        self.assertFalse('other' in obj.keys())
        self.assertFalse('host.cluster' in obj.keys())

    def test_augment_query_option(self):
        target_cmd = [ './ci_test_macros', '0', 'spot,region.count,local_query="select scale(sum#region.count,10)",cross_query="select sum(scale#sum#region.count) as tenXcount",output=stdout' ]
        query_cmd = [ '../../src/tools/cali-query/cali-query', '-j' ]

        recs = json.loads(cat.run_test_with_query(target_cmd, query_cmd, {}))
        correct_recs = 0
        for rec in recs:
            if 'path' in rec:
                self.assertEqual(10*int(rec['sum#sum#rc.count']), int(rec['sum#scale#sum#region.count']))
                correct_recs = correct_recs + 1
        self.assertGreaterEqual(correct_recs, 2)

    def test_esc(self):
        """ Test proper character escaping when writing and reading .cali data """

        target_cmd = [ './ci_test_basic', 'newline' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-j' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        targets = {}
        for rec in obj:
            for key in rec.keys():
                if key.startswith('event.set#'):
                    targets[key] = rec[key]

        self.assertGreaterEqual(len(targets), 2)

        self.assertEqual(targets['event.set# =\\weird ""attribute"=  '], '  \\\\ weird," name",' )
        self.assertEqual(targets['event.set#newline'], 'A newline:\n!')

    def test_macros(self):
        """ Test the annotation macros """

        target_cmd = [ './ci_test_macros', '0', 'event-trace,output=stdout' ]

        out,_ = cat.run_test(target_cmd, {})
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 10)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main',
                'loop'   : 'main loop',
                'iteration#main loop' : '3',
                'region.count' : '1' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : ['main', 'foo', 'pre-loop', 'foo.init'] }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : ['main', 'before_loop'] }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : ['main', 'foo'],
                'loop'   : ['main loop', 'fooloop'],
                'iteration#fooloop' : '3' }))

    def test_property_override(self):
        """ Test CALI_CALIPER_ATTRIBUTE_PROPERTIES variable """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e', '--list-attributes' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
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
        """ Test the AnnotationBinding class """

        target_cmd = [ './ci_test_binding' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE': 'serial-trace',
            'CALI_RECORDER_FILENAME': 'stdout'
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'testbinding' : [ 'binding.nested=outer', 'binding.nested=inner' ] }))

    def test_inclusive_region_filter(self):
        """ Test region name filtering with the include_regions option """

        target_cmd = [ './ci_test_macros', '0', 'event-trace,include_regions="startswith(main)",output=stdout' ]

        out,_ = cat.run_test(target_cmd, {})
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

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
        """ Test region name filtering with the exclude_regions option """

        target_cmd = [ './ci_test_macros', '0', 'runtime-profile,exclude_regions="before_loop,inner_before_loop",output.format=cali,output=stdout' ]

        out,_ = cat.run_test(target_cmd, {})
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : [ 'main', 'foo' ],
                'loop'   : [ 'main loop', 'fooloop' ] }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : [ 'main', 'before_loop' ] }))

    def test_region_level_filter(self):
        """ Test region level filter """

        target_cmd = [ './ci_test_macros', '0', 'runtime-profile,level=phase,output=stdout' ]

        out,_ = cat.run_test(target_cmd, {})
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : [ 'main', 'foo' ],
                'loop'   : [ 'main loop', 'fooloop' ] }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : [ 'main', 'bar' ],
                'phase'  : 'after_loop' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : 'main',
                'phase'  : 'after_loop' }))

    def test_branch_filter(self):
        """ Test region branch filter """
        target_cmd = [ './ci_test_macros', '0', 'runtime-profile,include_branches=after_loop,output=stdout' ]

        out,_ = cat.run_test(target_cmd, None)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : [ 'main', 'foo' ],
                'loop'   : [ 'main loop', 'fooloop' ] }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {
                'region' : ['main', 'bar' ],
                'phase'  : 'after_loop' }))

    def test_alloc(self):
        """ Test memory region tracking in alloc service """

        target_cmd = [ './ci_test_alloc' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'        : 'alloc:recorder:trace:event',
            'CALI_EVENT_ENABLE_SNAPSHOT_INFO' : 'false',
            'CALI_ALLOC_RESOLVE_ADDRESSES': 'true',
            'CALI_RECORDER_FILENAME'      : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

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

    def test_measurement_template(self):
        """ Test the measurement service template """

        target_cmd = [ './ci_test_basic' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,measurement_template,trace,recorder',
            'CALI_MEASUREMENT_TEMPLATE_NAMES' : 'ci_test',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)
        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'myphase',
                         'measurement.val.ci_test',
                         'measurement.ci_test' }))


class CaliperCAPITest(unittest.TestCase):
    """ Caliper C API test cases """

    def test_c_ann_trace(self):
        target_cmd = [ './ci_test_c_ann', 'event-trace,output=stdout' ]

        out,_ = cat.run_test(target_cmd, None)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) >= 10)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, {'iteration', 'phase', 'time.duration.ns', 'global.int' }))
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
        target_cmd = [ './ci_test_c_ann', 'event-trace,output=stdout' ]

        out,_ = cat.run_test(target_cmd, None)
        _,globals = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertIn('global.double', globals)
        self.assertIn('global.string', globals)
        self.assertIn('global.int', globals)
        self.assertIn('global.uint', globals)
        self.assertIn('cali.caliper.version', globals)

        self.assertEqual(globals['global.int'], '1337')
        self.assertEqual(globals['global.string'], 'my global string')
        self.assertEqual(globals['global.uint'], '42')

    def test_c_ann_metadata(self):
        target_cmd = [ './ci_test_c_ann' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e', '--list-attributes',
                       '--print-attributes', 'cali.attribute.name,cali.attribute.type,meta-attr' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
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

        out,_ = cat.run_test(target_cmd, { 'CALI_CONFIG': 'event-trace,output=stdout' })
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) >= 4)
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'ci_test_c': 'snapshot', 'string_arg': 'teststring', 'int_arg': '42' }))

    def test_channel_c_api(self):
        target_cmd = [ './ci_test_channel_api' ]

        out,_ = cat.run_test(target_cmd, None)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region': 'foo',
                         'a': '2',
                         'b': '4' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region': 'foo',
                         'b': '4',
                         'c': '8' }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'region': 'foo',
                         'a': '2',
                         'b': '4',
                         'c': '8' }))


class CaliperLogTest(unittest.TestCase):
    """ Caliper Log test cases """

    def test_log_verbose(self):
        target_cmd = [ './ci_test_basic' ]

        env = { 'CALI_LOG_VERBOSITY' : '3',
                'CALI_LOG_LOGFILE'   : 'stdout'
        }

        log_targets = [
            '== CALIPER: Releasing Caliper thread data',
            'Process blackboard',
            'Thread blackboard',
            'Metadata tree',
            'Metadata memory pool'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_cali_config_error_msg(self):
        target_cmd = [ './ci_test_basic' ]

        env = {
            'CALI_LOG_VERBOSITY' : '0',
            'CALI_LOG_LOGFILE'   : 'stdout',
            'CALI_CONFIG'        : 'blagarbl'
        }

        log_targets = [
            '== CALIPER: CALI_CONFIG: error: Unknown config or parameter: blagarbl'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_scope_parse_error_msgs(self):
        target_cmd = [ './ci_test_basic' ]

        env = {
            'CALI_LOG_VERBOSITY' : '0',
            'CALI_LOG_LOGFILE'   : 'stdout',
            'CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE' : 'bar'
        }

        log_targets = [
            'Invalid value "bar" for CALI_CALIPER_ATTRIBUTE_DEFAULT_SCOPE'
        ]

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_log_silent(self):
        target_cmd = [ './ci_test_basic' ]

        env = { 'CALI_LOG_VERBOSITY' : '0',
                'CALI_LOG_LOGFILE'   : 'stdout'
        }

        report_out,_ = cat.run_test(target_cmd, env)
        lines = report_out.decode().splitlines()

        self.assertTrue(len(lines) == 0)


class CaliperTestMonitor(unittest.TestCase):
    """ Caliper test case """

    def test_loopmonitor_iter_interval(self):
        target_cmd = [ './ci_test_macros', '0', 'none', '10' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'loop_monitor,trace,report',
            'CALI_LOOP_MONITOR_ITERATION_INTERVAL' : '5',
            'CALI_REPORT_CONFIG'     : 'select * where iteration#main\\ loop format cali',
            'CALI_REPORT_FILENAME'   : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) == 2)
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'iteration#main loop' : '4',
                         'loop.iterations'     : '5'
            }))

    def test_loopmonitor_time_interval(self):
        target_cmd = [ './ci_test_macros', '2000', 'none', '10' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'loop_monitor,trace,report',
            'CALI_LOOP_MONITOR_TIME_INTERVAL' : '0.05',
            'CALI_REPORT_CONFIG'     : 'select count(),sum(loop.iterations) where loop format json',
            'CALI_REPORT_FILENAME'   : 'stdout',
        }

        query_output = cat.run_test(target_cmd, caliper_config)
        obj = json.loads( query_output[0] )

        self.assertTrue(len(obj) == 1)

        count = int(obj[0]['count'])
        sum   = int(obj[0]['sum#loop.iterations'])

        self.assertTrue(count > 2 and count < 8)
        self.assertEqual(sum, 10)

    def test_loopmonitor_target_loop(self):
        target_cmd = [ './ci_test_macros', '0', 'none', '10' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'loop_monitor,trace,report',
            'CALI_LOOP_MONITOR_ITERATION_INTERVAL' : '5',
            'CALI_LOOP_MONITOR_TARGET_LOOPS'       : 'fooloop',
            'CALI_REPORT_CONFIG'     : 'select * where iteration#fooloop format cali',
            'CALI_REPORT_FILENAME'   : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertEqual(len(snapshots), 20)
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'iteration#main loop' : '3',
                         'iteration#fooloop'   : '4',
                         'loop.iterations'     : '5',
                         'loop'                : [ 'main loop', 'fooloop' ]
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'iteration#main loop' : '7',
                         'iteration#fooloop'   : '9',
                         'loop.iterations'     : '5',
                         'loop'                : [ 'main loop', 'fooloop' ]
            }))

    def test_regionmonitor_time_interval(self):
        target_cmd = [ './ci_test_macros', '5000', 'none', '5' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'region_monitor,trace,report',
            'CALI_REGION_MONITOR_TIME_INTERVAL' : '0.01',
            'CALI_REPORT_CONFIG'     : 'select * format cali',
            'CALI_REPORT_FILENAME'   : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) == 4)
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'loop'   : [ 'main loop', 'fooloop' ],
                         'region' : [ 'main', 'foo' ]
            }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : [ 'main', 'pre-loop' ] }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : [ 'main', 'before_loop' ] }))

if __name__ == "__main__":
    unittest.main()

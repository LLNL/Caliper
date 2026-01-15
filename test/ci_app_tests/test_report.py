# report / C config test

import json,unittest

import calipertest as cat

class CaliperReportTest(unittest.TestCase):
    """ Caliper Report / Reader CSV export / C config API test case """

    def test_report_caliwriter(self):
        """ Test reader lib's .cali export via report service """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),iteration#fooloop WHERE loop=fooloop GROUP BY iteration#fooloop FORMAT expand' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'format cali',
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) == 5)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'iteration#fooloop': '3', 'count': '4' }))

    def test_report_aggregate(self):
        """ Test reader lib's CSV export via report service """

        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,aggregate,report',
            'CALI_REPORT_CONFIG'     : 'select region,count(),inclusive_sum(count) group by region format expand',
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region': 'main', 'count': '19', 'inclusive#count': '83' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region': 'main/foo', 'count': '48', 'inclusive#count': '60' }))

    def test_report_nested_key(self):
        """ Test "group by path" """

        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() group by path,iteration#main\\ loop format expand',
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'loop'   : 'main loop',
                         'region' : 'main/foo/pre-loop/foo.init',
                         'iteration#main loop' : '2',
                         'count'  : '1'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : 'main/foo',
                         'loop'   : 'main loop/fooloop',
                         'iteration#main loop' : '3',
                         'count'  : '9'
            }))

    def test_report_class_iteration(self):
        """ Test "group by class.iteration" """

        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() group by path,class.iteration format expand',
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : 'main/foo',
                         'loop'   : 'main loop/fooloop',
                         'iteration#main loop' : '3',
                         'iteration#fooloop'  : '1',
                         'count'      : '1'
            }))

    def test_report_aggregation_aliases(self):
        """ Test aliases (select ... as )"""

        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() as my\\ count\\ alias group by path,iteration#main\\ loop format expand',
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'loop'   : 'main loop',
                         'region' : 'main/foo/pre-loop/foo.init',
                         'iteration#main loop' : '2',
                         'my count alias'      : '1'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region' : 'main/foo',
                         'loop'   : 'main loop/fooloop',
                         'iteration#main loop' : '3',
                         'my count alias'      : '9'
            }))

    def test_report_tree_formatter(self):
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,sum(region.count) as Count group by path where region.count format tree',
        }

        report_targets = [
            'Path                  Count',
            'main                      1',
            '  main loop               5',
            '    foo                   4',
            '      fooloop            20',
            '      pre-loop            4',
            '        foo.init          4',
            '  before_loop             1',
            '    inner_before_loop     1'
        ]

        report_output,_ = cat.run_test(target_cmd, caliper_config)
        lines = [ l.rstrip() for l in report_output.decode().splitlines() ]
        for target in report_targets:
            if not target in lines:
                self.fail('%s not found in report' % target)

    def test_report_table_formatter(self):
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() group by loop where event.end#loop format table order by count desc',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        report_targets = [
            'loop              count',
            'main loop/fooloop     4',
            'main loop             1'
        ]

        report_output,_ = cat.run_test(target_cmd, caliper_config)
        output_lines = report_output.decode().splitlines()

        for lineno in [ 0, 1, 2]:
            self.assertEqual(report_targets[lineno], output_lines[lineno].rstrip())

    def test_report_attribute_aliases(self):
        """ Test aliases in expand formatter """

        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select region as my\\ function\\ alias,count() group by region format expand',
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_keys(snapshots, { 'my function alias', 'count'  }))

    def test_report_attribute_aliases_in_cali(self):
        """ Test aliases in json-split formatter """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-q', 'format json(object)' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_FILENAME'   : 'stdout',
            'CALI_REPORT_CONFIG'     : 'select *,count() as CountAlias group by path format cali',
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config ) )

        self.assertTrue( { 'records', 'globals', 'attributes' }.issubset(set(obj.keys())) )

        self.assertIn('count',           obj['attributes'])
        self.assertIn('attribute.alias', obj['attributes']['count'])

        self.assertEqual('CountAlias', obj['attributes']['count']['attribute.alias'])


class CaliperRuntimeReportTest(unittest.TestCase):
    """ Runtime report controller """
    def test_runtime_report_default(self):
        target_cmd = [ './ci_test_macros', '10', 'runtime-report,output=stdout' ]

        log_targets = [
            'Path',
            'main',
            '  main loop',
            '    foo',
            '      fooloop'
        ]

        report_out,_ = cat.run_test(target_cmd, {})
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_runtime_report_nompi(self):
        target_cmd = [ './ci_test_macros', '10', 'runtime-report,aggregate_across_ranks=false,output=stdout,max_column_width=0' ]

        log_targets = [
            'Time (E) Time (I) Time % (E) Time % (I)',
            'main',
            '  main loop',
            '    foo',
            '      fooloop'
        ]

        report_out,_ = cat.run_test(target_cmd, {})
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_runtime_report_max_column_width(self):
        target_cmd = [ './ci_test_macros', '10', 'runtime-report,output=stdout,max_column_width=8' ]

        log_targets = [
            'Path     ',
            'main     ',
            '  ma~~op ',
            '    foo  ',
            '      fo ',
            '      .. ',
        ]

        report_out,_ = cat.run_test(target_cmd, {})
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)


class CaliperLoopReportTest(unittest.TestCase):
    """ Loop report controller (summary info) """
    def test_loopreport_summary(self):
        target_cmd = [ './ci_test_macros', '10', 'loop-report,summary=true,timeseries=false,output=stdout', '20' ]

        log_targets = [
            'Loop summary:',
            'Loop      Iterations Time (s)',
            'main loop         20'
        ]

        report_out,_ = cat.run_test(target_cmd, {})
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    """ Loop report controller (timeseries info) """
    def test_loopreport_timeseries(self):
        target_cmd = [ './ci_test_macros', '0', 'loop-report,timeseries=true,summary=false,iteration_interval=15,output=stdout', '75' ]

        log_targets = [
            'Iteration summary (main loop):',
            'Block Iterations Time (s)',
            '    0         15',
            '   30         15',
            '   60         15'
        ]

        report_out,_ = cat.run_test(target_cmd, {})
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    """ Loop report controller (timeseries info, display default number of rows (20)) """
    def test_loopreport_display_default_num_rows(self):
        target_cmd = [ './ci_test_macros', '0', 'loop-report,timeseries=true,summary=false,iteration_interval=2,output=stdout', '80' ]

        report_out,_ = cat.run_test(target_cmd, {})
        lines = report_out.decode().splitlines()

        self.assertGreater(len(lines), 20)
        self.assertLess(len(lines), 28)

    """ Loop report controller (timeseries info, display all rows) """
    def test_loopreport_display_all_rows(self):
        target_cmd = [ './ci_test_macros', '0', 'loop-report,timeseries.maxrows=0,timeseries=true,summary=false,iteration_interval=2,output=stdout', '80' ]

        report_out,_ = cat.run_test(target_cmd, {})
        lines = report_out.decode().splitlines()

        self.assertGreater(len(lines), 40)
        self.assertLess(len(lines), 48)

    """ Loop report controller (timeseries info, target loop selection) """
    def test_loopreport_target_loop_selection(self):
        target_cmd = [ './ci_test_macros', '0', 'loop-report,target_loops=fooloop,timeseries=true,summary=true,iteration_interval=5,output=stdout,timeseries.maxrows=0', '20' ]

        log_targets = [
            'Iteration summary (fooloop):',
            'main loop/fooloop        400',
            'Block Iterations Time (s)',
            '    0        100',
            '    5        100',
            '   10        100'
        ]

        report_out,_ = cat.run_test(target_cmd, {})
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    """ Loop statistics option """
    def test_loop_stats(self):
        target_cmd = [ './ci_test_macros', '10', 'spot,loop.stats,output=stdout' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-q', 'let r=leaf() select * where r=main\\ loop format json' ]

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, None) )

        self.assertEqual(len(obj), 1)

        rec = obj[0]

        self.assertTrue("max#max#max#iter.count" in rec)
        self.assertTrue("avg#avg#ls.avg" in rec)
        self.assertTrue("min#min#ls.min" in rec)
        self.assertTrue("max#max#ls.max" in rec)

        self.assertEqual(int(rec["max#max#max#iter.count"]), 4)
        self.assertGreaterEqual(float(rec["min#min#ls.min"]), 0.000009)
        self.assertGreaterEqual(float(rec["avg#avg#ls.avg"]), float(rec["min#min#ls.min"]))
        self.assertGreaterEqual(float(rec["max#max#ls.max"]), float(rec["avg#avg#ls.avg"]))


class CaliperJSONTest(unittest.TestCase):
    """ Caliper JSON formatters test cases """

    def test_jsonobject(self):
        """ Test json object layout """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),* group by path where region format json(object)' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config ) )

        self.assertTrue( { 'records', 'globals', 'attributes' }.issubset(set(obj.keys())) )

        self.assertEqual(len(obj['records']), 10)
        self.assertTrue('path' in obj['records'][0].keys())

        self.assertTrue('cali.caliper.version' in obj['globals'].keys())
        self.assertTrue('count' in obj['attributes'].keys())
        self.assertEqual(obj['attributes']['cali.caliper.version']['is_global'], 1)

    def test_jsonobject_pretty(self):
        """ Test json object layout """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),* group by path where region format json(object,pretty)' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config ) )

        self.assertTrue( { 'records', 'globals', 'attributes' }.issubset(set(obj.keys())) )

        self.assertEqual(len(obj['records']), 10)
        self.assertTrue('path' in obj['records'][0].keys())

        self.assertTrue('cali.caliper.version' in obj['globals'].keys())
        self.assertTrue('count' in obj['attributes'].keys())
        self.assertEqual(obj['attributes']['cali.caliper.version']['is_global'], True)

    def test_jsontree(self):
        """ Test basic json-tree formatter """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),sum(time.inclusive.duration.ns),loop,iteration#main\\ loop group by loop,iteration#main\\ loop format json-split' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        self.assertTrue( { 'data', 'columns', 'column_metadata', 'nodes' }.issubset(set(obj.keys())) )

        columns = obj['columns']

        self.assertEqual( { 'path', 'iteration#main loop', 'count', 'sum#time.inclusive.duration.ns' }, set(columns) )

        data = obj['data']

        self.assertEqual(len(data), 10)
        self.assertEqual(len(data[0]), 4)

        meta = obj['column_metadata']

        self.assertEqual(len(meta), 4)
        self.assertTrue(meta[columns.index('count')]['is_value'])
        self.assertFalse(meta[columns.index('path')]['is_value'])

        nodes = obj['nodes']

        self.assertEqual(nodes[0]['label' ], 'main loop')
        self.assertEqual(nodes[0]['column'], 'path')
        self.assertEqual(nodes[1]['label' ], 'fooloop')
        self.assertEqual(nodes[1]['column'], 'path')
        self.assertEqual(nodes[1]['parent'], 0)

        iterindex = columns.index('iteration#main loop')

        # Note: this is a pretty fragile test
        self.assertEqual(data[9][iterindex], 3)

    def test_runtime_profile(self):
        """ Test runtime-profile controller """

        target_cmd = [ './ci_test_macros', '0', 'runtime-profile,use.mpi=false,output=stdout,output.format=json-split,node.order=false' ]

        obj = json.loads( cat.run_test(target_cmd, {})[0] )

        self.assertTrue( { 'data', 'columns', 'column_metadata', 'nodes' }.issubset(set(obj.keys())) )

        columns = obj['columns']

        self.assertEqual( { 'path', 'time' }, set(columns) )

        data = obj['data']

        self.assertEqual(len(data), 11)
        self.assertEqual(len(data[0]), 2)

        meta = obj['column_metadata']

        self.assertEqual(len(meta), 2)

        time_idx = columns.index('time')
        path_idx = columns.index('path')

        self.assertTrue( meta[time_idx]['is_value'])
        self.assertEqual(meta[time_idx]['attribute.unit'], 'sec')
        self.assertFalse(meta[path_idx]['is_value'])

    def test_esc(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'select *,count() format json-split' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        columns = obj['columns']

        self.assertTrue('event.set# =\\weird ""attribute"=  ' in columns)

        data  = obj['data']
        nodes = obj['nodes']

        index = columns.index('event.set# =\\weird ""attribute"=  ')

        self.assertEqual(nodes[data[0][index]]['label'], '  \\\\ weird," name",' )
        self.assertEqual(obj[' =\\weird "" global attribute"=  '], '  \\\\ weird," name",')

if __name__ == "__main__":
    unittest.main()

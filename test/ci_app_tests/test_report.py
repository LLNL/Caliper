# report / C config test

import json,unittest

import calipertest as cat

class CaliperReportTest(unittest.TestCase):
    """ Caliper Report / Reader CSV export / C config API test case """

    def test_report_caliwriter(self):
        """ Test reader lib's CSV export via report service """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),iteration#fooloop WHERE loop=fooloop GROUP BY iteration#fooloop FORMAT expand' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'format cali',
            'CALI_LOG_VERBOSITY'     : '0'
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
            'CALI_REPORT_CONFIG'     : 'select function,count(),inclusive_sum(count) group by function format expand',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function': 'main', 'count': '17', 'inclusive#count': '77' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function': 'main/foo', 'count': '60', 'inclusive#count': '60' }))

    def test_report_nested_key(self):
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() group by prop:nested,iteration#main\ loop format expand',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function'   : 'main/foo',
                         'loop'       : 'main loop',
                         'annotation' : 'pre-loop',
                         'statement'  : 'foo.init',
                         'iteration#main loop' : '2',
                         'count'      : '1'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function'   : 'main/foo',
                         'loop'       : 'main loop/fooloop',
                         'iteration#main loop' : '3',
                         'count'      : '9'
            }))

    def test_report_class_iteration(self):
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() group by prop:nested,class.iteration format expand',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function'   : 'main/foo',
                         'loop'       : 'main loop/fooloop',
                         'iteration#main loop' : '3',
                         'iteration#fooloop'  : '1',
                         'count'      : '1'
            }))

    def test_report_aggregation_aliases(self):
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() as my\\ count\\ alias group by prop:nested,iteration#main\ loop format expand',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function'   : 'main/foo',
                         'loop'       : 'main loop',
                         'annotation' : 'pre-loop',
                         'statement'  : 'foo.init',
                         'iteration#main loop' : '2',
                         'my count alias'      : '1'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function'   : 'main/foo',
                         'loop'       : 'main loop/fooloop',
                         'iteration#main loop' : '3',
                         'my count alias'      : '9'
            }))

    def test_report_tree_formatter(self):
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() as Count group by prop:nested where cali.event.end format tree',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        report_targets = [
            'Path             Count',
            'main                 1',
            '  main loop          5',
            '    foo              4',
            '      fooloop       20',
            '      pre-loop       4',
            '        foo.init     4',
            '  before_loop        1'
        ]

        report_output,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_output.decode().splitlines()

        for target in report_targets:
            for line in lines:
                if target in line:
                    break
            else:
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
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select function as my\\ function\\ alias,count() group by function format expand',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_keys(snapshots, { 'my function alias', 'count'  }))

    def test_report_attribute_aliases_in_cali(self):
        """ Test json object layout """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-q', 'format json(object)' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_FILENAME'   : 'stdout',
            'CALI_REPORT_CONFIG'     : 'select *,count() as CountAlias group by prop:nested format cali',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config ) )

        self.assertTrue( { 'records', 'globals', 'attributes' }.issubset(set(obj.keys())) )

        self.assertIn('count',           obj['attributes'])
        self.assertIn('attribute.alias', obj['attributes']['count'])

        self.assertEqual('CountAlias', obj['attributes']['count']['attribute.alias'])

    def test_report(self):
        target_cmd = [ './ci_test_report' ]
        # create some distraction: read-from-env should be disabled
        env = { 'CALI_SERVICES_ENABLE' : 'debug' }

        report_out,_ = cat.run_test(target_cmd, env)

        lines = report_out.decode().splitlines()

        self.assertEqual(lines[0].rstrip(), 'function annotation count')
        self.assertEqual(lines[1].rstrip(), 'main     my phase       2')

if __name__ == "__main__":
    unittest.main()

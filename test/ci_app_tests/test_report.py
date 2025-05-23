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
            'CALI_REPORT_CONFIG'     : 'select region,count(),inclusive_sum(count) group by region format expand',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output,_ = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region': 'main', 'count': '19', 'inclusive#count': '83' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region': 'main/foo', 'count': '48', 'inclusive#count': '60' }))

    def test_report_nested_key(self):
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() group by path,iteration#main\\ loop format expand',
            'CALI_LOG_VERBOSITY'     : '0'
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
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() group by path,class.iteration format expand',
            'CALI_LOG_VERBOSITY'     : '0'
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
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select *,count() as my\\ count\\ alias group by path,iteration#main\\ loop format expand',
            'CALI_LOG_VERBOSITY'     : '0'
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
            'CALI_LOG_VERBOSITY'     : '0'
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
        target_cmd = [ './ci_test_macros' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,trace,report',
            'CALI_REPORT_CONFIG'     : 'select region as my\\ function\\ alias,count() group by region format expand',
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
            'CALI_REPORT_CONFIG'     : 'select *,count() as CountAlias group by path format cali',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config ) )

        self.assertTrue( { 'records', 'globals', 'attributes' }.issubset(set(obj.keys())) )

        self.assertIn('count',           obj['attributes'])
        self.assertIn('attribute.alias', obj['attributes']['count'])

        self.assertEqual('CountAlias', obj['attributes']['count']['attribute.alias'])

if __name__ == "__main__":
    unittest.main()

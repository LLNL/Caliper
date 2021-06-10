# Loop report tests

import json
import os
import subprocess
import unittest

import calipertest as cat


class CaliperSpotControllerTest(unittest.TestCase):
    def test_spot_regionprofile(self):
        target_cmd = [ './ci_test_macros', '0', 'spot(output=stdout,region.count)' ]
        query_cmd = [ '../../src/tools/cali-query/cali-query', '-q', 'format json(object)' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        self.assertIn('globals', obj)
        self.assertIn('records', obj)

        self.assertIn('spot.format.version', obj['globals'])
        self.assertIn('avg#inclusive#sum#time.duration', obj['globals']['spot.metrics'])
        self.assertIn('min#inclusive#sum#time.duration', obj['globals']['spot.metrics'])
        self.assertIn('max#inclusive#sum#time.duration', obj['globals']['spot.metrics'])
        self.assertIn('sum#inclusive#sum#time.duration', obj['globals']['spot.metrics'])
        self.assertIn('min#sum#rc.count',                obj['globals']['spot.metrics'])
        self.assertIn('avg#sum#rc.count',                obj['globals']['spot.metrics'])
        self.assertIn('max#sum#rc.count',                obj['globals']['spot.metrics'])
        self.assertIn('sum#sum#rc.count',                obj['globals']['spot.metrics'])
        self.assertIn('avg#inclusive#sum#time.duration', obj['attributes'])
        self.assertIn('min#inclusive#sum#time.duration', obj['attributes'])
        self.assertIn('max#inclusive#sum#time.duration', obj['attributes'])
        self.assertIn('sum#inclusive#sum#time.duration', obj['attributes'])
        self.assertIn('attribute.alias', obj['attributes']['avg#inclusive#sum#time.duration'])
        self.assertIn('attribute.unit',  obj['attributes']['avg#inclusive#sum#time.duration'])

        self.assertEqual('Avg time/rank', obj['attributes']['avg#inclusive#sum#time.duration']['attribute.alias'])
        self.assertEqual('sec',           obj['attributes']['avg#inclusive#sum#time.duration']['attribute.unit'])

        r = None
        for rec in obj['records']:
            if 'path' in rec and rec['path'] == 'main/main loop/foo':
                r = rec
                break

        self.assertTrue(r is not None, 'No record with "path" entry found')
        self.assertIn('avg#inclusive#sum#time.duration', r)
        self.assertIn('min#inclusive#sum#time.duration', r)
        self.assertIn('max#inclusive#sum#time.duration', r)
        self.assertIn('sum#inclusive#sum#time.duration', r)
        self.assertEqual(int(r['sum#sum#rc.count']), 4)
        self.assertEqual(r['spot.channel'], 'regionprofile')

        self.assertEqual('regionprofile', obj['globals']['spot.channels'])

    def test_spot_timeseries(self):
        target_cmd = [ './ci_test_macros', '0', 'spot(output=stdout,timeseries,timeseries.iteration_interval=15)', '75' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-q', 'select * where spot.channel=timeseries format json(object)' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'      : '0',
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        self.assertIn('globals', obj)
        self.assertIn('records', obj)

        self.assertIn('timeseries', obj['globals']['spot.options'])
        self.assertIn('timeseries', obj['globals']['spot.channels'])
        self.assertIn('regionprofile', obj['globals']['spot.channels'])

        self.assertIn('avg#ratio#loop.iterations/time.duration', obj['globals']['spot.timeseries.metrics'])
        self.assertIn('avg#ratio#loop.iterations/time.duration', obj['records'][0])
        self.assertEqual(obj['records'][0]['spot.channel'], 'timeseries')

        blocks = []
        for rec in obj['records']:
            blocks.append(int(rec['block']))
        blocks.sort()

        self.assertEqual(blocks, [ 0, 15, 30, 45, 60 ])

    def test_spot_optioncheck_a(self):
        target_cmd = [ './ci_test_macros', '0', 'spot(output=stdout,timeseries.iteration_interval=15)' ]

        env = {
            'CALI_LOG_VERBOSITY' : '0',
        }

        log_targets = [
            'timeseries.iteration_interval is set but the timeseries option is not enabled'
        ]

        target_proc = subprocess.Popen(target_cmd, env=env, stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        _,report_err = target_proc.communicate()

        lines = report_err.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_spot_optioncheck_b(self):
        target_cmd = [ './ci_test_macros', '0', 'spot(output=stdout,timeseries,timeseries.metrics=blagarbl)' ]

        env = {
            'CALI_LOG_VERBOSITY' : '0',
        }

        log_targets = [
            'Unknown option: blagarbl'
        ]

        target_proc = subprocess.Popen(target_cmd, env=env, stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        _,report_err = target_proc.communicate()

        lines = report_err.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

if __name__ == "__main__":
    unittest.main()

# Test the loop and region monitor services

import json
import unittest

import calipertest as cat

class CaliperTestMonitor(unittest.TestCase):
    """ Caliper test case """

    def test_loopmonitor_iter_interval(self):
        target_cmd = [ './ci_test_macros', '0', 'none', '10' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'loop_monitor,trace,report',
            'CALI_LOOP_MONITOR_ITERATION_INTERVAL' : '5',
            'CALI_REPORT_CONFIG'     : 'select * where iteration#mainloop format expand',
            'CALI_REPORT_FILENAME'   : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output[0])

        self.assertTrue(len(snapshots) == 2)
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'iteration#mainloop' : '4', 
                         'loop.iterations'    : '5' 
            }))

    def test_loopmonitor_time_interval(self):
        target_cmd = [ './ci_test_macros', '2000', 'none', '10' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'loop_monitor,trace,report',
            'CALI_LOOP_MONITOR_TIME_INTERVAL' : '0.05',
            'CALI_REPORT_CONFIG'     : 'select count(),sum(loop.iterations) where loop format json',
            'CALI_REPORT_FILENAME'   : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test(target_cmd, caliper_config)
        obj = json.loads( query_output[0] )

        self.assertTrue(len(obj) == 1)

        count = int(obj[0]['count'])
        sum   = int(obj[0]['sum#loop.iterations'])

        self.assertTrue(count > 2 and count < 8)
        self.assertEqual(sum, 10)
    
    def test_regionmonitor_time_interval(self):
        target_cmd = [ './ci_test_macros', '5000', 'none', '5' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'region_monitor,trace,report',
            'CALI_REGION_MONITOR_TIME_INTERVAL' : '0.01',
            'CALI_REPORT_CONFIG'     : 'select * format expand',
            'CALI_REPORT_FILENAME'   : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test(target_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output[0])

        self.assertTrue(len(snapshots) == 4)
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'loop'     : 'mainloop/fooloop', 
                         'function' : 'main/foo' 
            }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'annotation' : 'pre-loop' }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'annotation' : 'before_loop' }))

if __name__ == "__main__":
    unittest.main()

# Multi-experiment tests

import unittest

import calipertest as cat

class CaliperMultiExperimentTest(unittest.TestCase):
    """ Caliper multi-experiment case """

    def test_multiexperiment_trace(self):
        target_cmd = [ './ci_test_multiexperiment' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) >= 205)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'exp.id'       : '1', 
                        'thread'       : 'true' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'exp.id'       : '1', 
                        'main'         : 'true' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'exp.id'       : '42', 
                        'thread'       : 'true' }))

    def test_multiexperiment_aggr(self):
        target_cmd = [ './ci_test_multiexperiment' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate,event,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) >= 210)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'exp.id'       : '1', 
                        'thread'       : 'true' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'exp.id'       : '1', 
                        'main'         : 'true' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'exp.id'       : '42', 
                        'thread'       : 'true' }))

if __name__ == "__main__":
    unittest.main()

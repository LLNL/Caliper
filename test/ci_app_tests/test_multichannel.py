# Multi-channel tests

import unittest

import calipertest as cat

class CaliperMultiChannelTest(unittest.TestCase):
    """ Caliper multi-channel case """

    def test_multichannel_trace(self):
        target_cmd = [ './ci_test_multichannel' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_CHANNEL_SNAPSHOT_SCOPES' : 'process,thread,channel',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) >= 205)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'chn.id'       : '1', 
                        'thread'       : 'true' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'chn.id'       : '1', 
                        'main'         : 'true' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'chn.id'       : '42', 
                        'thread'       : 'true' }))

    def test_multichannel_aggr(self):
        target_cmd = [ './ci_test_multichannel' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'aggregate,event,recorder',
            'CALI_CHANNEL_SNAPSHOT_SCOPES' : 'process,thread,channel',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) >= 210)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'chn.id'       : '1', 
                        'thread'       : 'true' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'chn.id'       : '1', 
                        'main'         : 'true' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, {'chn.id'       : '42', 
                        'thread'       : 'true' }))

    def test_channel_c_api(self):
        target_cmd = [ './ci_test_channel_api' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY' : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'annotation': 'foo',
                         'a': '2',
                         'b': '4' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'annotation': 'foo',
                         'b': '4',
                         'c': '8' }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'annotation': 'foo',
                         'a': '2',
                         'b': '4',
                         'c': '8' }))
        

if __name__ == "__main__":
    unittest.main()

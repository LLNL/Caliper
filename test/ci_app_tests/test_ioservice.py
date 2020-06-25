# IoService tests

import json
import unittest

import calipertest as cat

class CaliperIoServiceTest(unittest.TestCase):
    """ Caliper I/O service test case """

    def test_ioservice(self):
        target_cmd = [ './ci_test_io', 'event-trace(trace.io=true),output=stdout' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'io.bytes.read': '16',
                         'io.region':     'read',
                         'function':      'main' }))

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'io.bytes.read',
                         'io.region',
                         'io.filesystem',
                         'io.mount.point' }))

    def test_ioservice_multichannel_disabled(self):
        # Enable the IO service in the default config but not in the trace channel.
        # While I/O regions will be on the shared blackboard, there should be no
        # I/O event records in the trace channel.

        target_cmd = [ './ci_test_io', 'event-trace(trace.io=false,output=stdout)' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0',
            'CALI_SERVICES_ENABLE'   : 'event,io,trace',
            'CALI_CHANNEL_CONFIG_CHECK' : 'false'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function': 'main' }))
        self.assertFalse(cat.has_snapshot_with_keys(
            snapshots, { 'io.region' }))

    def test_hatchet_profile_ioservice(self):
        # Test the I/O bytes metric in the
        target_cmd = [ './ci_test_io', 'hatchet-region-profile,use.mpi=false,output=stdout,output.format=json,io.bytes.read=true' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test(target_cmd, caliper_config)[0] )

        self.assertEqual(obj[1]['path'], 'main')
        self.assertEqual(int(obj[1]['Bytes read']), 16)

if __name__ == "__main__":
    unittest.main()

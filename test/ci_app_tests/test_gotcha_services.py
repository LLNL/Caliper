# IoService tests

import io
import json
import unittest

import caliperreader
import calipertest as cat

class CaliperIoServiceTest(unittest.TestCase):
    """ Caliper I/O service test case """

    def test_ioservice(self):
        target_cmd = [ './ci_test_io', 'event-trace(trace.io=true),output=stdout' ]

        out,_ = cat.run_test(target_cmd, None)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'io.bytes.read': '16',
                         'io.region':     'read',
                         'region':    'main' }))

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

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0',
            'CALI_SERVICES_ENABLE'   : 'event,io,trace',
            'CALI_CHANNEL_CONFIG_CHECK' : 'false'
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'region': 'main' }))
        self.assertFalse(cat.has_snapshot_with_keys(
            snapshots, { 'io.region' }))

    def test_runtime_profile_ioservice(self):
        # Test the I/O bytes metric
        target_cmd = [ './ci_test_io', 'runtime-profile,use.mpi=false,output=stdout,output.format=json,io.bytes.read=true' ]

        obj = json.loads( cat.run_test(target_cmd, None)[0] )

        self.assertEqual(obj[1]['path'], 'main')
        self.assertEqual(int(obj[1]['Bytes read']), 16)

    def test_pthread(self):
            target_cmd = [ './ci_test_thread' ]

            caliper_config = {
                'CALI_SERVICES_ENABLE'   : 'event,pthread,trace,recorder',
                'CALI_RECORDER_FILENAME' : 'stdout',
            }

            out,_ = cat.run_test(target_cmd, caliper_config)
            snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

            self.assertTrue(len(snapshots) >= 20)

            self.assertTrue(cat.has_snapshot_with_keys(
                snapshots, {'local', 'global', 'region'}))
            self.assertTrue(cat.has_snapshot_with_keys(
                snapshots, {'pthread.id', 'pthread.is_master', 'my_thread_id', 'global', 'region'}))
            self.assertTrue(cat.has_snapshot_with_attributes(
                snapshots, {'my_thread_id' : '49',
                            'pthread.is_master' : 'false',
                            'region'     : 'thread_proc',
                            'global'       : '999' }))
            self.assertTrue(cat.has_snapshot_with_attributes(
                snapshots, { 'region'    : 'main',
                            'pthread.is_master' : 'true',
                            'local'       : '99' }))


if __name__ == "__main__":
    unittest.main()

# Thread tests

import io
import unittest

import caliperreader
import calipertest as cat

class CaliperThreadTest(unittest.TestCase):
    """ Caliper thread test case """

    def test_thread(self):
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

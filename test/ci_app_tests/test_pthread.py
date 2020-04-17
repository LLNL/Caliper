# Thread tests

import unittest

import calipertest as calitest

class CaliperThreadTest(unittest.TestCase):
    """ Caliper thread test case """

    def test_thread(self):
        target_cmd = [ './ci_test_thread' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'thread-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) >= 20)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, {'local', 'global', 'function'}))
        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, {'pthread.id', 'pthread.is_master', 'my_thread_id', 'global', 'function'}))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, {'my_thread_id' : '49',
                        'pthread.is_master' : 'false',
                        'function'     : 'thread_proc',
                        'global'       : '999' }))
        self.assertTrue(calitest.has_snapshot_with_attributes(
            snapshots, { 'function'    : 'main',
                         'pthread.is_master' : 'true',
                         'local'       : '99' }))

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, {'pthread.id', 'my_thread_id', 'global', 'pthread_exp'}))
        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, {'global', 'my_thread_id', 'nopthread_exp'}))
        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, {'event.set#pthread.id', 'pthread_exp'}))
        self.assertFalse(calitest.has_snapshot_with_keys(
            snapshots, {'event.set#pthread.id', 'nopthread_exp'}))

if __name__ == "__main__":
    unittest.main()

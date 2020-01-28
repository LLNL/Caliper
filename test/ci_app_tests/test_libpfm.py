# Libpfm tests

import unittest

import calipertest as calitest

class CaliperLibpfmTest(unittest.TestCase):
    """ Caliper Libpfm test case """

    def test_libpfm_counting(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'          : 'event:libpfm:trace:recorder',
            'CALI_LIBPFM_ENABLE_SAMPLING'   : 'false',
            'CALI_LIBPFM_RECORD_COUNTERS'   : 'true',
            'CALI_LIBPFM_EVENTS'            : 'cycles',
            'CALI_LIBPFM_SAMPLE_PERIOD'     : '10000000',
            'CALI_LIBPFM_SAMPLE_ATTRIBUTES' : 'ip,time',
            'CALI_RECORDER_FILENAME'        : 'stdout',
            'CALI_LOG_VERBOSITY'            : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'libpfm.counter.cycles', 'phase', 'iteration' }))

    def test_libpfm_sampling(self):
        target_cmd = [ './ci_test_macros', '50', 'none', '100' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'        : 'event:libpfm:pthread:trace:recorder',
            'CALI_LIBPFM_ENABLE_SAMPLING' : 'true',
            'CALI_LIBPFM_RECORD_COUNTERS' : 'false',
            'CALI_LIBPFM_EVENTS'          : 'instructions',
            'CALI_RECORDER_FILENAME'      : 'stdout',
            'CALI_LOG_VERBOSITY'          : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'libpfm.event_sample_name', 'libpfm.ip', 'libpfm.time' }))

if __name__ == "__main__":
    unittest.main()

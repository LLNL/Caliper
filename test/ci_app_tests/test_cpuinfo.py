# CpuInfo tests

import unittest

import calipertest as calitest

class CaliperCpuInfoTest(unittest.TestCase):
    """ Caliper CpuInfo test case """

    def test_papi(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,cpuinfo,trace,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'cpuinfo.cpu',
                         'cpuinfo.numa_node',
                         'phase',
                         'iteration' }))


if __name__ == "__main__":
    unittest.main()

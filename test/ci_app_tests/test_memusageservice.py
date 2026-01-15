# MemUsage tests

import io
import unittest

import caliperreader
import calipertest as cat

class CaliperMemusageServiceTest(unittest.TestCase):
    """ Caliper MemUsage service test case """

    def test_memstat(self):
        target_cmd = [ './ci_test_basic' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,memstat,trace,recorder',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'memstat.vmsize',
                         'memstat.data',
                         'myphase',
                         'iteration' }))

if __name__ == "__main__":
    unittest.main()

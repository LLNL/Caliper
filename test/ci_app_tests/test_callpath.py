# callpath service tests

import io
import unittest

import caliperreader
import calipertest as cat

class CaliperCallpathTest(unittest.TestCase):
    """ Caliper callpath smoketest """

    def test_callpath(self):
        target_cmd = [ './ci_test_alloc' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'callpath,trace,recorder',
            'CALI_CALLPATH_USE_NAME' : 'true',
            'CALI_RECORDER_FILENAME' : 'stdout',
        }

        out,_ = cat.run_test(target_cmd, caliper_config)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) == 3)

        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'callpath.address', 'callpath.regname', 'test_alloc.allocated.0', 'region' }))

        sreg = cat.get_snapshot_with_keys(snapshots, { 'callpath.regname', 'test_alloc.allocated.0' })

        self.assertTrue('main' in sreg.get('callpath.regname'))

if __name__ == "__main__":
    unittest.main()

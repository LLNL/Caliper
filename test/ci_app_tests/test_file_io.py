# File and directory creation test cases

import os
import unittest

import calipertest as cat

class CaliperFileIOTest(unittest.TestCase):
    """ Caliper file and directory creation test case """

    def test_createdir(self):
        target_cmd = [ './ci_test_macros', '0', 'hatchet-region-profile,use.mpi=false,output.format=json,output=foo/bar/test.json' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        if os.path.exists('foo/bar/test.json'):
            os.remove('foo/bar/test.json')
        if os.path.exists('foo/bar'):
            os.removedirs('foo/bar')

        self.assertFalse(os.path.exists('foo/bar'))

        cat.run_test(target_cmd, caliper_config)

        self.assertTrue(os.path.isdir('foo/bar'))
        self.assertTrue(os.path.isfile('foo/bar/test.json'))

        if os.path.exists('foo/bar/test.json'):
            os.remove('foo/bar/test.json')
        if os.path.exists('foo/bar'):
            os.removedirs('foo/bar')


if __name__ == "__main__":
    unittest.main()

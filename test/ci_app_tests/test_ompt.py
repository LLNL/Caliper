# OmptService tests

import json
import unittest

import calipertest as cat

class CaliperOpenMPMetrics(unittest.TestCase):
    """ Caliper OpenMP/OMPT test case """

    def test_ioservice(self):
        target_cmd = [ './ci_test_openmp', 'hatchet-region-profile,openmp.times,output=stdout' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test(target_cmd, caliper_config)[0] )

        self.assertTrue( { 'data', 'columns', 'column_metadata', 'nodes' }.issubset(set(obj.keys())) )

        expected = { 
            'path', 
            'time', 
            'OpenMP Barriers', 
            'OpenMP Work'
        }

        self.assertTrue(expected.issubset(set(obj['columns'])))

if __name__ == "__main__":
    unittest.main()

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
            'Time (barrier)', 
            'Time (work)'
        }

        self.assertTrue(expected.issubset(set(obj['columns'])))

    def test_openmpreport_regions(self):
        target_cmd = [ './ci_test_openmp', 'openmp-report,output=stdout' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        report_targets = [
            'Path #Threads Time (thread) Time (total) Work %   Barrier % Time (work) Time (barrier)'
        ]

        report_output,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_output.decode().splitlines()

        for target in report_targets:
            for line in lines:
                if target in line:
                    break

if __name__ == "__main__":
    unittest.main()

# OmptService tests

import json
import unittest

import calipertest as cat

class CaliperOpenMPMetrics(unittest.TestCase):
    """ Caliper OpenMP/OMPT test case """

    def test_ioservice(self):
        target_cmd = [ './ci_test_openmp', 'runtime-profile,openmp.times,output=stdout,output.format=json-split' ]

        obj = json.loads( cat.run_test(target_cmd, None)[0] )

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

        report_targets = [
            'Path #Threads Time (thread) Time (total) Work %   Barrier % Time (work) Time (barrier)'
        ]

        report_output,_ = cat.run_test(target_cmd, None)
        lines = report_output.decode().splitlines()

        for target in report_targets:
            for line in lines:
                if target in line:
                    break

if __name__ == "__main__":
    unittest.main()

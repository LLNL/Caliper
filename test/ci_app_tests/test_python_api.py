# Tests of the Python API

import io
import sys
import unittest

import caliperreader
import calipertest as cat


class CaliperPythonAPITest(unittest.TestCase):
    """Caliper Python API test cases"""

    def test_py_ann_trace(self):
        target_cmd = [
            sys.executable,
            "./ci_test_py_ann.py",
            "event-trace,output=stdout",
        ]

        out,_ = cat.run_test(target_cmd, None)
        snapshots,_ = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertTrue(len(snapshots) >= 10)

        self.assertTrue(
            cat.has_snapshot_with_keys(
                snapshots, {"iteration", "phase", "time.duration.ns", "global.int"}
            )
        )
        self.assertTrue(
            cat.has_snapshot_with_attributes(
                snapshots, {"event.end#phase": "loop", "phase": "loop"}
            )
        )
        self.assertTrue(
            cat.has_snapshot_with_attributes(
                snapshots,
                {"event.end#iteration": "3", "iteration": "3", "phase": "loop"},
            )
        )
        self.assertTrue(
            cat.has_snapshot_with_keys(
                snapshots,
                {"attr.int", "attr.dbl", "attr.str", "ci_test_c_ann.setbyname"},
            )
        )
        self.assertTrue(
            cat.has_snapshot_with_attributes(
                snapshots, {"attr.int": "20", "attr.str": "fidibus"}
            )
        )

    def test_py_ann_globals(self):
        target_cmd = [sys.executable, "./ci_test_py_ann.py"]

        out,_ = cat.run_test(target_cmd, { 'CALI_CONFIG': 'event-trace,output=stdout' })
        _,globals = caliperreader.read_caliper_contents(io.StringIO(out.decode()))

        self.assertIn("global.double", globals)
        self.assertIn("global.string", globals)
        self.assertIn("global.int", globals)
        self.assertIn("global.uint", globals)
        self.assertIn("cali.caliper.version", globals)

        self.assertEqual(globals['global.int'], '1337')
        self.assertEqual(globals['global.string'], 'my global string')
        self.assertEqual(globals['global.uint'], '42')


if __name__ == "__main__":
    unittest.main()

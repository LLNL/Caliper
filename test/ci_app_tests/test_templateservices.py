# Template service tests

import unittest

import calipertest as calitest

class CaliperTemplateServicesTest(unittest.TestCase):
    """ Caliper template services test cases """

    def test_measurement_template(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'CALI_SERVICES_ENABLE'   : 'event,measurement_template,trace,recorder',
            'CALI_MEASUREMENT_TEMPLATE_NAMES' : 'ci_test',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        query_output = calitest.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = calitest.get_snapshots_from_text(query_output)

        self.assertTrue(len(snapshots) > 1)

        self.assertTrue(calitest.has_snapshot_with_keys(
            snapshots, { 'phase',
                         'measurement.val.ci_test',
                         'measurement.ci_test' }))

if __name__ == "__main__":
    unittest.main()

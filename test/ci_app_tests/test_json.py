# report / C config test

import json
import unittest

import calipertest as cat

class CaliperJSONTest(unittest.TestCase):
    """ Caliper JSON formatters test cases """

    def test_jsontree(self):
        """ Test basic json-tree formatter """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),sum(time.inclusive.duration),loop,iteration#mainloop group by loop,iteration#mainloop format json-tree' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        self.assertTrue( { 'data', 'columns', 'nodes' }.issubset(set(obj.keys())) )

        columns = obj['columns']

        self.assertEqual( { 'path', 'iteration#mainloop', 'count', 'time.inclusive.duration' }, set(columns) )

        data = obj['data']

        self.assertEqual(len(data), 7)
        self.assertEqual(len(data[0]), 4)

        nodes = obj['nodes']

        self.assertEqual(nodes[0]['label'],  'mainloop')
        self.assertEqual(nodes[1]['label'],  'fooloop')
        self.assertEqual(nodes[1]['parent'], 0)

        iterindex = columns.index('iteration#mainloop')

        self.assertEqual(data[6][iterindex], 3)

        
if __name__ == "__main__":
    unittest.main()

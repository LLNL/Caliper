# JSON output test cases

import json
import unittest

import calipertest as cat

class CaliperJSONTest(unittest.TestCase):
    """ Caliper JSON formatters test cases """

    def test_jsonobject(self):
        """ Test json object layout """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),* group by prop:nested where function format json(object)' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config ) )

        self.assertTrue( { 'records', 'globals', 'attributes' }.issubset(set(obj.keys())) )

        self.assertEqual(len(obj['records']), 7)
        self.assertTrue('path' in obj['records'][0].keys())

        self.assertTrue('cali.caliper.version' in obj['globals'].keys())
        self.assertTrue('count' in obj['attributes'].keys())
        self.assertEqual(obj['attributes']['cali.caliper.version']['is_global'], 1)        

        
    def test_jsonobject_pretty(self):
        """ Test json object layout """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),* group by prop:nested where function format json(object,pretty)' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config ) )

        self.assertTrue( { 'records', 'globals', 'attributes' }.issubset(set(obj.keys())) )

        self.assertEqual(len(obj['records']), 7)
        self.assertTrue('path' in obj['records'][0].keys())

        self.assertTrue('cali.caliper.version' in obj['globals'].keys())
        self.assertTrue('count' in obj['attributes'].keys())
        self.assertEqual(obj['attributes']['cali.caliper.version']['is_global'], True)

        
    def test_jsontree(self):
        """ Test basic json-tree formatter """

        target_cmd = [ './ci_test_macros' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'SELECT count(),sum(time.inclusive.duration),loop,iteration#main\ loop group by loop,iteration#main\ loop format json-split' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        self.assertTrue( { 'data', 'columns', 'column_metadata', 'nodes' }.issubset(set(obj.keys())) )

        columns = obj['columns']

        self.assertEqual( { 'path', 'iteration#main loop', 'count', 'sum#time.inclusive.duration' }, set(columns) )

        data = obj['data']

        self.assertEqual(len(data), 10)
        self.assertEqual(len(data[0]), 4)

        meta = obj['column_metadata']

        self.assertEqual(len(meta), 4)
        self.assertTrue(meta[columns.index('count')]['is_value'])
        self.assertFalse(meta[columns.index('path')]['is_value'])

        nodes = obj['nodes']

        self.assertEqual(nodes[0]['label' ], 'main loop')
        self.assertEqual(nodes[0]['column'], 'path')
        self.assertEqual(nodes[1]['label' ], 'fooloop')
        self.assertEqual(nodes[1]['column'], 'path')
        self.assertEqual(nodes[1]['parent'], 0)

        iterindex = columns.index('iteration#main loop')

        # Note: this is a pretty fragile test
        self.assertEqual(data[9][iterindex], 3)

        
    def test_hatchetcontroller(self):
        """ Test hatchet-region-profile controller """

        target_cmd = [ './ci_test_macros', '0', 'hatchet-region-profile,use.mpi=false,output=stdout' ]

        caliper_config = {
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test(target_cmd, caliper_config)[0] )

        self.assertTrue( { 'data', 'columns', 'column_metadata', 'nodes' }.issubset(set(obj.keys())) )

        columns = obj['columns']

        self.assertEqual( { 'path', 'time' }, set(columns) )

        data = obj['data']

        self.assertEqual(len(data), 8)
        self.assertEqual(len(data[0]), 2)

        meta = obj['column_metadata']

        self.assertEqual(len(meta), 2)

        time_idx = columns.index('time')
        path_idx = columns.index('path')

        self.assertTrue( meta[time_idx]['is_value'])
        self.assertEqual(meta[time_idx]['attribute.unit'], 'sec')
        self.assertFalse(meta[path_idx]['is_value'])


    def test_esc(self):
        target_cmd = [ './ci_test_basic' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query',
                       '-q', 'select *,count() format json-split' ]

        caliper_config = {
            'CALI_CONFIG_PROFILE'    : 'serial-trace',
            'CALI_RECORDER_FILENAME' : 'stdout',
            'CALI_LOG_VERBOSITY'     : '0'
        }

        obj = json.loads( cat.run_test_with_query(target_cmd, query_cmd, caliper_config) )

        columns = obj['columns']

        self.assertTrue('event.set# =\\weird ""attribute"=  ' in columns)

        data  = obj['data']
        nodes = obj['nodes']

        index = columns.index('event.set# =\\weird ""attribute"=  ')

        self.assertEqual(nodes[data[0][index]]['label'], '  \\\\ weird," name",' )
        self.assertEqual(obj[' =\\weird "" global attribute"=  '], '  \\\\ weird," name",')

        
if __name__ == "__main__":
    unittest.main()

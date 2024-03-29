# Copyright (c) 2020, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

import unittest

import caliperreader as cr

class CaliperReaderBasic(unittest.TestCase):
    """ Basic test cases for the Caliper reader """

    def test_reader_class(self):
        r = cr.CaliperReader()

        r.read('example-profile.cali')

        self.assertEqual(len(r.records), 25)
        self.assertEqual(len(r.globals), 22)

        self.assertEqual(r.globals['spot.format.version'], '2')
        self.assertEqual(r.globals['threads'], '2')

        self.assertIn('path', r.records[4])
        self.assertIn('function', r.records[5])
        self.assertIn('avg#inclusive#sum#time.duration', r.records[6])

        self.assertTrue( r.attribute('function').is_nested())
        self.assertFalse(r.attribute('function').is_value() )

        attr = r.attribute("avg#inclusive#sum#time.duration")

        self.assertFalse(attr.is_nested())
        self.assertFalse(attr.is_global())
        self.assertTrue( attr.is_value() )
        self.assertFalse(attr.is_hidden())

        self.assertIsNotNone(attr.get('attribute.unit'))

        self.assertEqual(r.attribute('function').attribute_type(), 'string')
        self.assertEqual(r.attribute('figure_of_merit').get('adiak.type'), 'double')

        self.assertTrue(r.attribute('avg.sum#inclusive#sum#time.duration').is_hidden())

        meta = r.attribute('figure_of_merit').metadata()

        self.assertEqual( meta['adiak.type'], 'double' )
        self.assertEqual( meta['is_value'  ], False    )
        self.assertEqual( meta['is_global' ], True     )
        self.assertEqual( meta['type'      ], 'double' )

        self.assertIsNone(r.attribute('function').get('attribute.unit'))
        self.assertIsNone(r.attribute('function').get('DOES NOT EXIST'))
        self.assertEqual (r.attribute('function').scope(), 'process')

        attributes = r.attributes()

        self.assertIn('function', attributes)
        self.assertIn('avg#inclusive#sum#time.duration', attributes)
        self.assertIn('figure_of_merit', attributes)
        self.assertNotIn('avg.sum#inclusive#sum#time.duration', attributes)

    def test_read_contents(self):
        records, globals = cr.read_caliper_contents('example-profile.cali')

        self.assertEqual(len(records), 25)
        self.assertEqual(len(globals), 22)

        self.assertEqual(globals['cali.channel'],   'spot')
        self.assertEqual(globals['mpi.world.size'], '1')
        self.assertEqual(globals['problem_size'],   '30')

        self.assertIn('path',     records[7])
        self.assertIn('function', records[8])
        self.assertIn('avg#inclusive#sum#time.duration', records[9])
        # don't add hidden attributes
        self.assertNotIn('avg.sum#inclusive#sum#time.duration', records[9])

    def test_read_globals(self):
        globals = cr.read_caliper_globals('example-profile.cali')

        self.assertEqual(len(globals), 22)

        self.assertIn('libraries', globals)
        self.assertIn('elapsed_time', globals)
        self.assertIn('cali.caliper.version', globals)

        self.assertEqual(globals['region_balance'], '1')
        self.assertEqual(globals['user'], 'david')

if __name__ == "__main__":
    unittest.main()

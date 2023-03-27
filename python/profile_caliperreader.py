#!/usr/bin/env python3

import sys
import cProfile

import caliperreader

file = sys.argv[1]

cr = caliperreader.CaliperReader()
cProfile.run("cr.read(file)")

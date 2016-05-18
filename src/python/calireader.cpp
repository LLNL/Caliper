// Copyright (c) 2015-2016, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov; Alfredo Gimenez, gimemez1@llnl.gov
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file calireader.cpp
/// Caliper-to-python converter

// Include Python.h first because it may redefine some C stdlib macros
#include <Python.h>

#include <SimpleReader.h>
using namespace cali;

#include <string>

extern "C" {

PyObject* read_cali_file(const char *filename) {

    PyObject *result = PyDict_New();

    SimpleReader sr;
    sr.open(std::string(filename));

    long i = 0;
    ExpandedRecordMap rec;

    while (sr.nextSnapshot(rec)) {

        PyObject *row = PyDict_New();

        for (auto attr : rec) {
#if PY_MAJOR_VERSION < 3
            PyObject *key = PyString_FromString(attr.first.c_str());
            PyObject *value = PyString_FromString(attr.second.to_string().c_str());
#else
            PyObject *key = PyUnicode_FromString(attr.first.c_str());
            PyObject *value = PyUnicode_FromString(attr.second.to_string().c_str());            
#endif
            PyDict_SetItem(row, key, value);
        }

#if PY_MAJOR_VERSION < 3        
        PyObject *rowkey = PyInt_FromLong(i++);
#else
        PyObject *rowkey = PyLong_FromLong(i++);
#endif
        
        PyDict_SetItem(result, rowkey, row);
    }

    return result;
}

} // extern "C"

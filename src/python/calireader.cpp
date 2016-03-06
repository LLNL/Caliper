#include <SimpleReader.h>
using namespace cali;

#include <string>

#include <python2.6/Python.h>

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
            PyObject *key = PyString_FromString(attr.first.c_str());
            PyObject *value = PyString_FromString(attr.second.to_string().c_str());

            PyDict_SetItem(row, key, value);
        }

        PyObject *rowkey = PyInt_FromLong(i++);

        PyDict_SetItem(result, rowkey, row);
    }

    return result;
}

} // extern "C"

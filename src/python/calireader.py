from ctypes import cdll, py_object
lib = cdll.LoadLibrary('./libcalireader.so')

lib.read_cali_file.restype = py_object

def read_cali_file(filename):
        return lib.read_cali_file(filename)
    

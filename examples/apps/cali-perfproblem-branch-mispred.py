# Copyright (c) 2024, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.

from pycaliper.high_level import annotate_function
from pycaliper.annotation import Annotation

import numpy as np

@annotate_function()
def init(arraySize: int, sort: bool) -> np.array:
    data = np.random.randint(256, size=arraySize)
    if sort:
        data = np.sort(data)
    return data


@annotate_function()
def work(data: np.array):
    data_sum = 0
    for _ in range(100):
        for val in np.nditer(data):
            if val >= 128:
                data_sum += val
    print("sum =", data_sum)


@annotate_function()
def benchmark(arraySize: int, sort: bool):
    sorted_ann = Annotation("sorted")
    sorted_ann.set(sort)
    print("Intializing benchmark data with sort =", sort)
    data = init(arraySize, sort)
    print("Calculating sum of values >= 128")
    work(data)
    print("Done!")
    sorted_ann.end()
    

@annotate_function()
def main():
    arraySize = 32768
    benchmark(arraySize, True)
    benchmark(arraySize, False)
    

if __name__ == "__main__":
    main()
    

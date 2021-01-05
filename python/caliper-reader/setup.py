# Copyright (c) 2020-20201, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

import setuptools
from codecs import open
from os import path

here = path.abspath(path.dirname(__file__))

# Get the long description from the README file
with open(path.join(here, "README.md"), encoding="utf-8") as f:
    long_description = f.read()

# Get the version in a safe way which does not refrence the `__init__` file
# per python docs: https://packaging.python.org/guides/single-sourcing-package-version/
version = {}
with open("./caliperreader/version.py") as fp:
    exec(fp.read(), version)

setuptools.setup(
    name="caliper-reader",
    version=version["__version__"],
    description="A Python library for reading Caliper .cali files",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/LLNL/Caliper",
    author="David Boehme",
    author_email="boehme3@llnl.gov",
    license="BSD-3-Clause",
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "License :: OSI Approved :: BSD License",
    ],
    packages=setuptools.find_packages()
)

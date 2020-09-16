# Copyright (c) 2020, Lawrence Livermore National Security, LLC.
# See top-level LICENSE file for details.
#
# SPDX-License-Identifier: BSD-3-Clause

class ReaderError(Exception):
    def __init__(self, message):
        self.message = message

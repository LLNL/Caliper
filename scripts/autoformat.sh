#!/bin/bash

SUPPORTED_CLANG_FORMAT_VERSION="16.0.0"

if [ command -v clang-format >/dev/null 2>&1 ]; then
    echo "You must have 'clang-format' in PATH to use 'autoformat.sh'"
    exit 1
fi

clang_format_version_str=$(clang-format --version)
clang_format_version=$(echo "$clang_format_version_str" | grep -oP 'clang-format version \K\d+(\.\d+)+')

if [ "$clang_format_version" != "$SUPPORTED_CLANG_FORMAT_VERSION" ]; then
    echo "WARNING: the .clang-format file in this repo is designed for version 16.0.0."
    echo "         You are running with clang-format v$clang_format_version."
    echo "         The resulting formatting is highly likely to be incorrect."
fi

if [ command -v find >/dev/null 2>&1 ]; then
    echo "You must have 'find' in PATH to use 'autoformat.sh'"
    exit 1
fi

if [ command -v dirname >/dev/null 2>&1 ]; then
    echo "You must have 'dirname' in PATH to use 'autoformat.sh'"
    exit 1
fi

if [ command -v xargs >/dev/null 2>&1 ]; then
    echo "You must have 'dirname' in PATH to use 'autoformat.sh'"
    exit 1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

curr_dir=$(pwd)

cd $SCRIPT_DIR
cd ..

find src -name "*.c" -or -name "*.cpp" -or -name "*.h" -or -name "*.hpp" | xargs clang-format -i
find include -name "*.c" -or -name "*.cpp" -or -name "*.h" -or -name "*.hpp" | xargs clang-format -i
find examples -name "*.c" -or -name "*.cpp" -or -name "*.h" -or -name "*.hpp" | xargs clang-format -i
find test -name "*.c" -or -name "*.cpp" -or -name "*.h" -or -name "*.hpp" | xargs clang-format -i

cd $curr_dir
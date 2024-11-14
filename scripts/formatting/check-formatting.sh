#! /bin/bash

clang_format_exe="clang-format"
if [ $# -ge 1 ]; then
    clang_format_exe="$1"
fi

SUPPORTED_CLANG_FORMAT_VERSION="17.0.6"

if [ command -v $clang_format_exe >/dev/null 2>&1 ]; then
    echo "You must have 'clang-format' in PATH to use 'check-formatting.sh'"
    exit 1
fi

clang_format_version_str=$($clang_format_exe --version)
clang_format_version=$(echo "$clang_format_version_str" | grep -oP 'clang-format version \K\d+(\.\d+)+')

if [ "$clang_format_version" != "$SUPPORTED_CLANG_FORMAT_VERSION" ]; then
    echo "WARNING: the .clang-format file in this repo is designed for version $SUPPORTED_CLANG_FORMAT_VERSION."
    echo "         You are running with clang-format v$clang_format_version."
    echo "         The resulting check is highly likely to be incorrect."
fi

if [ command -v find >/dev/null 2>&1 ]; then
    echo "You must have 'find' in PATH to use 'check-formatting.sh'"
    exit 1
fi

if [ command -v dirname >/dev/null 2>&1 ]; then
    echo "You must have 'dirname' in PATH to use 'check-formatting.sh'"
    exit 1
fi

if [ command -v xargs >/dev/null 2>&1 ]; then
    echo "You must have 'dirname' in PATH to use 'check-formatting.sh'"
    exit 1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

curr_dir=$(pwd)

cd $SCRIPT_DIR
cd ..
cd ..

echo "Check formatting of C/C++ code in 'src'"
find src -name "*.c" -or -name "*.cpp" -or -name "*.h" -or -name "*.hpp" | xargs $clang_format_exe --dry-run -Werror

echo "Check formatting of C/C++ code in 'include'"
find include -name "*.c" -or -name "*.cpp" -or -name "*.h" -or -name "*.hpp" | xargs $clang_format_exe --dry-run -Werror

echo "Check formatting of C/C++ code in 'examples'"
find examples -name "*.c" -or -name "*.cpp" -or -name "*.h" -or -name "*.hpp" | xargs $clang_format_exe --dry-run -Werror

echo "Check formatting of C/C++ code in 'test'"
find test -name "*.c" -or -name "*.cpp" -or -name "*.h" -or -name "*.hpp" | xargs $clang_format_exe --dry-run -Werror

cd $curr_dir
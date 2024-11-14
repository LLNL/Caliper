#! /bin/bash

if [ command -v docker >/dev/null 2>&1 ]; then
    echo "Autoformatting with Docker requires Docker to be available"
    exit 1
fi

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

curr_dir=$(pwd)

cd $SCRIPT_DIR
cd ..
cd ..

echo "Bulding Docker image"
docker build -t caliper-autoformat -f ./scripts/formatting/Dockerfile.format .

echo "Running Docker container to autoformat code with clang-format 17.0.6"
docker run --rm -v $(pwd):/home/jovyan --name caliper-autoformat-container caliper-autoformat

cd $curr_dir
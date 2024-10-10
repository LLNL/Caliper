#! /bin/bash

if [ command -v docker >/dev/null 2>&1 ]; then
    echo "Formatting check with Docker requires Docker to be available"
    exit 1
fi

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

curr_dir=$(pwd)

cd $SCRIPT_DIR
cd ..
cd ..

echo "Bulding Docker image"
docker build -t caliper-check-format -f ./scripts/formatting/Dockerfile.check .

echo "Running Docker container to check formatting of code with clang-format 17.0.6"
docker run --rm -v $(pwd):/home/jovyan --name caliper-check-format-container caliper-check-format

cd $curr_dir
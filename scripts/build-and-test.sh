#!/usr/bin/env bash

hostname="$(hostname)"
timestamp="$(date +"%T" | sed 's/://g')"
project_dir="$(pwd)"

prefix="btests/${hostname}-${timestamp}"
echo "Creating directory ${prefix}"
echo "project_dir: ${project_dir}"

mkdir -p ${prefix}

python3 scripts/uberenv/uberenv.py --prefix ${prefix} --spec "%gcc"

hostconfig=()
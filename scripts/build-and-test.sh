#!/usr/bin/env bash

hostname="$(hostname)"
timestamp="$(date +"%T" | sed 's/://g')"
project_dir="$(pwd)"
hostname="$(hostname)"
truehostname=${hostname//[0-9]/}

prefix="btests/${hostname}-${timestamp}"
echo "Creating directory ${prefix}"
echo "project_dir: ${project_dir}"

mkdir -p ${prefix}

# generate cmake cache file with uberenv and radiuss spack package
python3 scripts/uberenv/uberenv.py --prefix ${prefix} --spec "%gcc"

# find generated cmake cache
hostconfigs=( $( ls "${project_dir}/"*.cmake ) )
if [[ ${#hostconfigs[@]} == 1 ]]
then
    hostconfig_path=${hostconfigs[0]}
    echo "Found host config file: ${hostconfig_path}"
elif [[ ${#hostconfigs[@]} == 0 ]]
then
    echo "No result for: ${project_dir}/*.cmake"
    echo "Spack generated host-config not found."
    exit 1
else
    echo "More than one result for: ${project_dir}/*.cmake"
    echo "${hostconfigs[@]}"
    echo "Please specify one with HOST_CONFIG variable"
    exit 1
fi

hostconfig=$(basename ${hostconfig_path})
echo "Found hostconfig: ${hostconfig}"

# build directory
build_dir="${prefix}/build_${hostconfig//.cmake/}"
install_dir="${prefix}/install_${hostconfig//.cmake/}"
cmake_exe=`grep 'CMake executable' ${hostconfig_path} | cut -d ':' -f 2 | xargs`


# Build
if [[ "${option}" != "--deps-only" && "${option}" != "--test-only" ]]
then
    date
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo "~~~~~ Host-config: ${hostconfig_path}"
    echo "~~~~~ Build Dir:   ${build_dir}"
    echo "~~~~~ Project Dir: ${project_dir}"
    echo "~~~~~ Install Dir: ${install_dir}"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo ""
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo "~~~~~ Building Caliper"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

    # Map CPU core allocations
    declare -A core_counts=(["lassen"]=40 ["ruby"]=28 ["corona"]=32 ["rzansel"]=48 ["tioga"]=32)

    # If building, then delete everything first
    # NOTE: 'cmake --build . -j core_counts' attempts to reduce individual build resources.
    #       If core_counts does not contain hostname, then will default to '-j ', which should
    #       use max cores.
    rm -rf ${build_dir} 2>/dev/null
    mkdir -p ${build_dir} && cd ${build_dir}

    date

    if [[ "${truehostname}" == "corona" || "${truehostname}" == "tioga" ]]
    then
        module unload rocm
    fi
    $cmake_exe \
      -C ${hostconfig_path} \
      -DCMAKE_INSTALL_PREFIX=${install_dir} \
      ${project_dir}
    if ! $cmake_exe --build . -j ${core_counts[$truehostname]}
    then
        echo "ERROR: compilation failed, building with verbose output..."
        $cmake_exe --build . --verbose -j 1
    else
        make install
    fi

    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo "~~~~~ Caliper Built"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    date
fi


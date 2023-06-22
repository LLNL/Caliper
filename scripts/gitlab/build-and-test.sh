#!/usr/bin/env bash

hostname="$(hostname)"
timestamp="$(date +"%T" | sed 's/://g')"
project_dir="$(pwd)"
hostname="$(hostname)"
spec=${SPEC:-""}
option=${1:-""}
truehostname=${hostname//[0-9]/}

prefix=""

if [[ "${option}" == "--project-build" ]]
then
    prefix=${project_dir}"/CI-builds/${hostname}-${timestamp}"
elif [[ -d /dev/shm ]]
then
    prefix="/dev/shm/${hostname}"
    if [[ -z ${job_unique_id} ]]; then
      job_unique_id=manual_job_$(date +%s)
      while [[ -d ${prefix}-${job_unique_id} ]] ; do
          sleep 1
          job_unique_id=manual_job_$(date +%s)
      done
    fi
fi

echo "Creating directory ${prefix}"
echo "project_dir: ${project_dir}"

mkdir -p ${prefix}

spack_user_cache="${prefix}/spack-user-cache"
export SPACK_DISABLE_LOCAL_CONFIG=""
export SPACK_USER_CACHE_PATH="${spack_user_cache}"
mkdir -p ${spack_user_cache}

if [[ -z ${spec} ]]
    then
        echo "SPEC is undefined, aborting..."
        exit 1
    fi

# generate cmake cache file with uberenv and radiuss spack package
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "~~~~~ Building Dependencies"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
./scripts/uberenv/uberenv.py --prefix ${prefix} --spec="${spec}"

# find generated cmake cache
if [[ -z ${hostconfig} ]]
then
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
else
    hostconfig_path = "${project_dir}/${hostconfig}"
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
    echo "~~~~~ Prefix: ${prefix}"
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
    if [[ "${truehostname}" == "lassen" ]]
    then
        $cmake_exe \
          -C ${hostconfig_path} \
          -DCMAKE_INSTALL_PREFIX=${install_dir} \
          -DRUN_MPI_TESTS=Off \
          ${project_dir}
    else
        $cmake_exe \
          -C ${hostconfig_path} \
          -DCMAKE_INSTALL_PREFIX=${install_dir} \
          ${project_dir}
    fi
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

# Test
if [[ "${option}" != "--build-only" ]]
then
    date
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo "~~~~~ Testing Caliper"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

    echo "Moving to ${build_dir}"
    cd ${build_dir}

    date
    ctest --output-on-failure -T test 2>&1 | tee tests_output.txt
    date


    no_test_str="No tests were found!!!"
    if [[ "$(tail -n 1 tests_output.txt)" == "${no_test_str}" ]]
    then
        echo "ERROR: No tests were found" && exit 1
    fi

    if grep -q "Errors while running CTest" ./tests_output.txt
    then
        echo "ERROR: failure(s) while running CTest" && exit 1
    fi

    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo "~~~~~ Caliper Tests Complete"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    date
fi

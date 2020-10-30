#!/bin/bash

BUILD_TYPE=release
BUILD_DIR=build
KERNEL_NAME=$(uname)
MACHINE_NAME=$(uname -m)

if [ ${KERNEL_NAME} = "Darwin" ] ; then
    export MACOSX_DEPLOYMENT_TARGET=10.9
fi 

function die()
{
    echo "Error: $*" >&2
    exit 1
}

function get_cpu_count()
{
    if [ ${KERNEL_NAME} = "Darwin" ] ; then
        CPU_COUNT=$(sysctl -n hw.ncpu)
    else
        CPU_COUNT=$(grep processor /proc/cpuinfo | wc -l)
    fi
}

function usage()
{
    echo "Usage: $0 [ debug | release ]" >&2
    echo "Default options: release" >&2
    exit 1
}

for arg in "$@"
do
    if [ $arg = "debug" -o $arg = "release" ]; then
        BUILD_TYPE=$arg
    else
        usage
    fi
done

echo "BUILD_TYPE: $BUILD_TYPE"
echo "kernel: $KERNEL_NAME"
echo "machine: $MACHINE_NAME"

get_cpu_count
if [ x$CPU_COUNT = x ] ; then
   CPU_COUNT=4
fi
echo "CPU count: ${CPU_COUNT}." 

echo "remove build directory"
rm -rf ${BUILD_DIR}
mkdir ${BUILD_DIR}

pushd ${BUILD_DIR}
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCPU_COUNT=$CPU_COUNT -DKERNEL_NAME=$KERNEL_NAME -DMACHINE_NAME=$MACHINE_NAME ..
make VERBOSE=1 -j ${CPU_COUNT}
popd

#!/bin/bash

BUILD_TYPE=release
BUILD_DIR=build
THIRDPARTY_PREFIX=external
UNAME=$(uname)
KERNEL_NAME=$(uname)
MACHINE_NAME=$(uname -m)
DEV_MODE="0"
OEM=

if [ ${UNAME} = "Darwin" ] ; then
    export MACOSX_DEPLOYMENT_TARGET=10.9
fi 

function die()
{
    echo "Error: $*" >&2
    exit 1
}

function get_cpu_count()
{
    if [ ${UNAME} = "Darwin" ] ; then
        CPU_COUNT=$(sysctl -n hw.ncpu)
    else
        CPU_COUNT=$(grep processor /proc/cpuinfo | wc -l)
    fi
}

function usage()
{
    echo "Usage: $0 [ debug | release ] [dev]" >&2
    exit 1
}

for arg in "$@"
do
    if [ $arg = "debug" -o $arg = "release" ]; then
        BUILD_TYPE=$arg
    elif [[ $arg =~ ^oem=.*$ ]] ; then
        OEM=${arg#oem=}
    elif [ $arg = "dev" ]; then
        DEV_MODE="1"
    else
        usage
    fi

done
echo "OEM=$OEM"
echo "BUILD_TYPE: $BUILD_TYPE"
echo "kernel: $KERNEL_NAME"
echo "machine: $MACHINE_NAME"
if [ x$CPU_COUNT = x ] ; then
get_cpu_count
fi
if [ x$CPU_COUNT = x ] ; then
   CPU_COUNT=4
fi
 
echo "CPU count: ${CPU_COUNT}." 

echo "install the dependency thirdpart libraries ..."
if [ ${UNAME} = "Linux" ] ; then
    LINUX_OS="Debian7_8"
    echo "Linux OS: $LINUX_OS"
fi

echo "remove build directory"
rm -rf ${BUILD_DIR}
mkdir ${BUILD_DIR}

ret=1
pushd ${BUILD_DIR}
if [ "${DEV_MODE}" == "1" ]; then
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DPLATFORM_TYPE="x86_64" -DCPU_COUNT=$CPU_COUNT -DKERNEL_NAME=$KERNEL_NAME -DMACHINE_NAME=$MACHINE_NAME -G Xcode ..
	ret=$?
else
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DPLATFORM_TYPE="x86_64" -DCPU_COUNT=$CPU_COUNT -DKERNEL_NAME=$KERNEL_NAME -DMACHINE_NAME=$MACHINE_NAME .. 
    make VERBOSE=1 -j ${CPU_COUNT}
    make install -j ${CPU_COUNT}
    make package
    ret=$?
fi
popd
exit ${ret}

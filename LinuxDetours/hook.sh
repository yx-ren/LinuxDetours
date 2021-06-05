#/bin/bash

EXEC_FILE=""
if [ $# -lt 1 ]; then
    echo "usage:$0 [exec_file_path]"
    exit 0
fi

EXEC_FILE=$1
if [ ! -f $EXEC_FILE ]; then
    echo "file:[$EXEC_FILE] not existed"
    exit 0
fi

echo "detected exec file:[$EXEC_FILE]"
curr_dir=$(pwd)
LD_PRELOAD="$curr_dir/hook.so" LD_LIBRARY_PATH=/usr/local/lib/ $EXEC_FILE
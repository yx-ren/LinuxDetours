#/bin/bash

make -j4
DUMP_SH="./dump_obj.sh"
if [ ! -f $DUMP_SH ];then
    echo "file:[$DUMP_SH] not existed"
    exit -1
fi

OBJS=("linux_detours.test" "libdetours64.so")

for ((i = 0; i != ${#OBJS[@]}; i++))
do
    obj=${OBJS[$i]}
    $DUMP_SH $obj
done

UNIT_TEST="./linux_detours.test"
UNIT_TEST_RESULT_FILE="unit_test_result"
$UNIT_TEST > $UNIT_TEST_RESULT_FILE 2>&1

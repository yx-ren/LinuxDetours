#/bin/bash

FUNCTION_NAME=trampoline_template_mips64
if [ $# -lt 1 ]; then
    echo "usage:$0 [obj_file_path] [function name(default = $FUNCTION_NAME)]"
    exit 0
fi

OBJ_FILE=$1
if [ ! -f $OBJ_FILE ]; then
    echo "file:[$OBJ_FILE] not existed"
    exit 0
fi

if [ $# -ge 2 ];then
    FUNCTION_NAME=$2
    echo "disassemble function name:[ $FUNCTION_NAME ]"
fi

start_line=$(grep -n -P "<\w+>" $OBJ_FILE | head -n 1 | cut -d ':' -f 1)
echo "start line:[ $start_line ]"
objdump_ins=$(sed -n "$start_line, $ p" $OBJ_FILE | awk \
                        ' \
                        { \
                            if (NF < 3 || length($2) != 8)
                                next

                            comments = "// "
                            inc = $2;
                            for (i = 1; i <= NF; i++)
                            {
                                comments = (comments""$i"    ");
                            }
                            printf("    array[i++] = 0x%s;   %s\n", inc, comments);
                        } \
                        ' \
             )
echo "$objdump_ins" | tee "$OBJ_FILE.disassemble"
exit 0

OBJ_START_LINES=$(grep -n -P "\<.*\>:" $OBJ_FILE | cut -d ':' -f 1)
FUNCTION_START_LINES=($OBJ_START_LINES) # to array
for ((i = 0; i != ${#FUNCTION_START_LINES[@]}; i++))
do
    line=${FUNCTION_START_LINES[i]}
    echo "line:[$line]"
    if [ $line == $start_line ]; then
        next_line=${FUNCTION_START_LINES[i+1]}
        end_line=$[$next_line - 1]
        begin_line=$[$start_line + 1]

        echo "start line:[ $begin_line ], end line:[ $end_line ]"
        objdump_ins=$(sed -n "$begin_line, $end_line p" $OBJ_FILE \
                        | awk \
                        ' \
                        { \
                            if (NF < 2)
                                next

                            comments = "// "
                            inc = ""
                            for (i = 2; i <= NF; i++)
                            {
                                inc = $2;
                                comments = (comments""$i"    ");
                            }
                            printf("    array[i++] = 0x%s;   %s\n", inc, comments);
                            #print comments
                        } \
                        ' \
                     )
        hard_code_incs_file="$FUNCTION_NAME.disassemble"
        echo "$objdump_ins" | tee $hard_code_incs_file
        echo "generate hard code into file:[ $hard_code_incs_file ]"
        break;
    fi
done

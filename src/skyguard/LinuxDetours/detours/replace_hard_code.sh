#/bin/bash
if [ $# -lt 2 ]; then
    echo "usage:$0 [hard code file] [cpp_file]"
    exit -1
fi

hard_code_file=$1
if [ ! -f $hard_code_file ]; then
    echo "disassemble file:[$hard_code_file] not existed"
    exit -1
fi

source_file=$2
if [ ! -f $source_file ]; then
    echo "source file:[$source_file] not existed"
    exit -1
fi

split_line_tag="this is a split comment"
split_lines=$(grep -n "$split_line_tag" $source_file | cut -d ':' -f1)
split_lines_ints=($split_lines) # to array
up_line=${split_lines_ints[0]}
down_line=${split_lines_ints[1]}

split_up_segment=$(sed -n "1, $up_line p" $source_file)
split_down_segment=$(sed -n "$down_line, $ p" $source_file)

target_file=$source_file
> $target_file
echo "$split_up_segment" >> $target_file
cat "$hard_code_file" >> $target_file
echo "$split_down_segment" >> $target_file

echo "replace file:[$source_file] (line[$up_line, $down_line]) down"

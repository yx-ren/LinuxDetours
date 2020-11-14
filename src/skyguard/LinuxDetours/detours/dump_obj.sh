#/bin/bash
if [ $# -lt 1 ]; then
    echo "usage:$0 [obj_file_path] [output path(default = current path)]"
    exit 0
fi

for arg in "$@"
do
    if [ ${arg%%=*} = "KEY1" ]; then
        if [ ${arg#*=} = "val1" ]; then
            echo "value is val1, "${arg#*=}""
        elif [ ${arg#*=} = "val2" ]; then
            echo "value is val2, "${arg#*=}""
        fi
    fi
done

for arg in $@
do
    obj_file=$arg
    if [ ! -f $obj_file ]; then
        echo "obj file [$obj_file] not existe, ignore dump"
        continue
    fi

    echo " dump object file:[$obj_file]"

    nm_dump="$obj_file.nm.dump"
    obj_dump="$obj_file.obj.dump"
    elf_dump="$obj_file.elf.dump"

    echo "nm dump -> $nm_dump"
    nm $obj_file > $nm_dump

    echo "objdump dump -> $obj_dump"
    objdump -D -j .text $obj_file > $obj_dump

    echo "elfdump dump -> $elf_dump"
    readelf -a $obj_file > $elf_dump
done

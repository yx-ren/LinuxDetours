#/bin/bash

regs_list=(rax rbx rcx rdx rsp rip rdi rsi r1 r2 r3 r4 r5 r6 r7 r8 r9)
file_name="trampoline_x86.cpp"
for ((i = 0; i != ${#regs_list[@]}; i++))
do
    echo "array[$i]:[${regs_list[$i]}]"
    #sed "s/%${regs_list[$i]}/%%%%%%%%%%%%%%${regs_list[$i]}/g" $file_name > dump

    #sed "s/\%${regs_list[$i]}/${regs_list[$i]}/g" $file_name > dump
    #sed "s/${regs_list[$i]}/%${regs_list[$i]}/g" dump > dump2

    sed -i "s/%${regs_list[$i]}/${regs_list[$i]}/g" $file_name
    sed -i "s/${regs_list[$i]}/%${regs_list[$i]}/g" $file_name
done

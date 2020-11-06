#include "detours.h"

#if defined(DETOURS_MIPS64)
__asm__
(R"(
.globl trampoline_template_mips64
.globl trampoline_data_mips64

NETIntro:
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0 
OldProc:
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
NewProc:
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
NETOutro:
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
IsExecutedPtr: 
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 0

trampoline_template_mips64:

        # TODO......
        # get pc instruction
        move $15,$31 # move t7,ra
        bal 4
        move $14,$31 # move t6,ra
        move $31,$15 # move ra,t7
        li $12, 12
#sub $13, $14, $12 # $13 save the base line
        dsubu $13, $14, $12 # $13 save the base line

        # get Hook Proc Address
        li $12, 12 # 12 is [NewProc] offset
        dsubu $11, $13, $12
        lw $10, 0($11)
#la $10, $11

        # jump Hook Proc
        jalr $11

        # -------------------- #

        jr $ra

        li      $2, 5001 #sys_write syscall id is 5001 -> v0
        li      $a0, 2 #fd -> a0
        li      $a1, SEGMENT1
        li      $a2, 3 
        syscall
        ld      $a3, NewProc
        li      $a3, NewProc

#move    $t9, $pc + NewProc
#addu    $t9, $pc, NewProc

#addu    $a1, $pc, NewProc
#move    $a1, $v0 + NewProc

#addu    $t9, $pc, NewProc
#jalr    $t9

###################################################################################### call original method
        j trampoline_exit

###################################################################################### call hook handler or original method...
call_net_entry:



## should call original method?

        j trampoline_exit

call_hook_handler:
## adjust return address

## call hook handler
    j trampoline_exit

call_net_outro: ## this is where the handler returns...

## call NET outro
    ## Here we are NOT under the alignment trick.


## finally return to saved return address - the caller of this trampoline...
    jr $ra

######################################################################################## generic outro for both cases...
trampoline_exit:


    ld $sp, 8($sp)
    j $t8

## outro signature, to automatically determine code size

trampoline_data_mips64:
    .byte 0x78
    .byte 0x56
    .byte 0x34
    .byte 0x12

SEGMENT1:
    .asciiz "abcdefg"


)");

#endif

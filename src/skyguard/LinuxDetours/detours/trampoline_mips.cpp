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

        # get pc instruction
        move $15,$31            # move t3,ra
        bal 4
        move $14,$31            # move t2,ra
        move $31,$15            # move ra,t3
        li $12, 12              # li t0,12    
        dsubu $13, $14, $12     # dsubu t1,t2,t0 // $t1 save the base line

        # -------------------- #

        # get Hook Proc Address
        li $12, 24              # li t0,24 //24 is [NewProc] offset from base line
        dsubu $13, $13, $12     # dsubu t1,t1,t0 // $t1 save the address of the [NewProc] address
        ld $12, 0($13)          # ld t0, t1 // read 8 byte from $t1, t0 save the [NewProc] address

        # -------------------- #

        # save the Remain address
        #daddiu  $sp, $sp, -16
        #sd $ra, 0($sp)

        # -------------------- #

        # jump Hook Proc
        move $25, $12           # save [NewProc] to $t9, because [$gp] calculate with $t9 in next stack
        jr $25                  # jr $t9

        # -------------------- #

        # jump to origin call
        # TODO...... (maybe)

        # -------------------- #

        # jump to Remain
        # TODO...... (maybe)

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

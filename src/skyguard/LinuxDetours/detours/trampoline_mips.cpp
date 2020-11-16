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
        li $12, 12              # move t0,12    
        dsubu $13, $14, $12     # t1 = t2 + t0 // $t1 save the base line

        # -------------------- #

        # store the registers and some value into stack
        daddiu  $sp, $sp, -128
        sd $ra, (0 * 8)($sp)    # save the Remain address
        sd $a0, (1 * 8)($sp)    # protected $a0 register (Arguments to functions)
        sd $a1, (2 * 8)($sp)    # protected $a1 register (Arguments to functions)
        sd $a2, (3 * 8)($sp)    # protected $a2 register (Arguments to functions)
        sd $a3, (4 * 8)($sp)    # protected $a3 register (Arguments to functions)
        sd $a4, (5 * 8)($sp)    # protected $a4 register (Arguments to functions)
        sd $t1, (6 * 8)($sp)    # save the base line(_DETOUR_TRAMPOLINE.rbTrampolineCode)
        sd $t9, (7 * 8)($sp)    # save the base line(_DETOUR_TRAMPOLINE.rbTrampolineCode)

        # -------------------- #

        # test segment #
#j call_hook_handler
        # test segment #

###################################################################################### call hook handler or original method...
## call NET intro
## TODO......
#    lea %rdi, [%rip + IsExecutedPtr + 8] ## Hook handle (only a position hint)
#    ## Here we are under the alignment trick.
#    mov %rdx, [%rsp + 32 + 8 * 16 + 6 * 8 + 8] ## %rdx = original %rsp (address of return address)
#    mov %rsi, [%rdx] ## return address (value stored in original %rsp)
#    call qword ptr [%rip + NETIntro] ## Hook->NETIntro(Hook, RetAddr, InitialRSP)##

## should call original method?
## TODO......
    # get arg1
    ld $a0, (6 * 8)($sp)    # restore base line to $a0

    # get arg2
    move $a1, $ra           # a1 save the return address

    # get arg3
    daddiu  $a2, $sp, 128   # a2 save the origin rsp

    # get [NETIntro] address
    ld $13, (6 * 8)($sp)    # restore base line to t1 
    li $12, 40              # li t0,40 //40 is [NETIntro] offset from base line
    dsubu $13, $13, $12     # dsubu t1,t1,t0 // $t1 save the address of the [NETIntro] address
    ld $12, 0($13)          # ld t0, t1 // read 8 byte from $t1, t0 save the [NETIntro] address

    # call [NETIntro]
    move $t9, $12
    jalr $t9
    bne $v0, 0, call_hook_handler

#bne $a1, $a1, call_hook_handler

    ## call original method
#li $13, 0xffffffff
    ld $13, (6 * 8)($sp)    # restore base line to t1 
    li $12, 32              # li t0,32 //32 is [OldProc] offset from base line
    dsubu $13, $13, $12     # dsubu t1,t1,t0 // $t1 save the address of the [OldProc] address
    ld $12, 0($13)          # ld t0, t1 // read 8 byte from $t1, t0 save the [OldProc] address
#move $25, $12              # in this case, t9 can't be modifed (jr $register is offset 8 byte after t9)
                            # so use anther register saved address and used to jump

    # -------------------- #

    # restore the registers
    ld $ra, (0 * 8)($sp)
    ld $a0, (1 * 8)($sp)
    ld $a1, (2 * 8)($sp)
    ld $a2, (3 * 8)($sp)
    ld $a3, (4 * 8)($sp)
    ld $a4, (5 * 8)($sp)
    ld $t9, (7 * 8)($sp)
    daddiu  $sp, $sp, 128

    # -------------------- #

    jr $12                  # jr $t0

call_hook_handler:
    # get Hook Proc Address and saved into $t9 register
#li $13, 0xffffffff
    ld $13, (6 * 8)($sp)    # restore base line to t1 
    li $12, 24              # li t0,24 //24 is [NewProc] offset from base line
    dsubu $13, $13, $12     # dsubu t1,t1,t0 // $t1 save the address of the [NewProc] address
    ld $12, 0($13)          # ld t0, t1 // read 8 byte from $t1, t0 save the [NewProc] address
    move $25, $12           # save [NewProc] to $t9, because [$gp] calculate with $t9 in next stack

    j trampoline_exit

###################################################################################### generic outro for both cases...
trampoline_exit:

        # -------------------- #

        # restore the registers
        ld $ra, (0 * 8)($sp)
        ld $a0, (1 * 8)($sp)
        ld $a1, (2 * 8)($sp)
        ld $a2, (3 * 8)($sp)
        ld $a3, (4 * 8)($sp)
        ld $a4, (5 * 8)($sp)
        ld $t9, (7 * 8)($sp)
        daddiu  $sp, $sp, 128

        # -------------------- #

        # jump to the address(address saved in $t9, [OldProc] or [HookProc])
        move $25, $12           # save [NewProc] to $t9, because [$gp] calculate with $t9 in next stack
        jr $25                  # jr $t9

######################################################################################
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

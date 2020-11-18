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
        daddiu $13, $14, -12    # t1 = t2 - 12 // $t1 save the base line, 12 is offset from base line to PC

        # -------------------- #


        # -------------------- #

        # store the registers and some value into stack
        ld $t3, ($sp)           # protected $sp 8 byte
        daddiu  $sp, $sp, -256
        sd $ra, (0 * 8)($sp)    # save the Remain address
        sd $a0, (1 * 8)($sp)    # protected $a0 register (Arguments to functions)
        sd $a1, (2 * 8)($sp)    # protected $a1 register (Arguments to functions)
        sd $a2, (3 * 8)($sp)    # protected $a2 register (Arguments to functions)
        sd $a3, (4 * 8)($sp)    # protected $a3 register (Arguments to functions)
        sd $a4, (5 * 8)($sp)    # protected $a4 register (Arguments to functions)
        sd $t1, (6 * 8)($sp)    # save the base line(_DETOUR_TRAMPOLINE.rbTrampolineCode)
        sd $t9, (7 * 8)($sp)    # save the $t9 register
        sd $sp, (8 * 8)($sp)    # save the $sp register
        sd $s0, (9 * 8)($sp)   #
        sd $s1, (10 * 8)($sp)   #
        sd $s2, (11 * 8)($sp)   #
        sd $s3, (12 * 8)($sp)   #
        sd $s4, (13 * 8)($sp)   #
        sd $s5, (14 * 8)($sp)   #
        sd $s6, (15 * 8)($sp)   #
        sd $s7, (16 * 8)($sp)   #
        sd $s8, (17 * 8)($sp)   #
        sd $t3, (18 * 8)($sp)   # import!!!, $ra will override this region

###################################################################################### call hook handler or original method...
## call NET intro
## TODO......
#    lea %rdi, [%rip + IsExecutedPtr + 8] ## Hook handle (only a position hint)
#    ## Here we are under the alignment trick.
#    mov %rdx, [%rsp + 32 + 8 * 16 + 6 * 8 + 8] ## %rdx = original %rsp (address of return address)
#    mov %rsi, [%rdx] ## return address (value stored in original %rsp)
#    call qword ptr [%rip + NETIntro] ## Hook->NETIntro(Hook, RetAddr, InitialRSP)##

## should call original method?
    # get arg1
    ld $a0, (6 * 8)($sp)    # a0 save restore base line

    # get arg2
    move $a1, $ra           # a1 save the return address

    # get arg3
    daddiu $a2, $sp, 256    # a2 save the origin rsp

    # get [NETIntro] address
    ld $13, (6 * 8)($sp)    # restore base line to $t1 
    daddiu $13, $13, -40    # t1 -= 40 // $t1 save the address of the [NETIntro] address
    ld $12, 0($13)          # ld t0, t1 // read 8 byte from $t1, t0 save the [NETIntro] address

    # call Hook->NETIntro(Hook, RetAddr, InitialRSP)
    move $t9, $12
    jalr $t9
    bne $v0, 0, call_hook_handler # if BarrierIntro() return true, call hook proc

    ## call original method
    ld $13, (6 * 8)($sp)    # restore base line to t1 
    daddiu $13, $13, -32    # t1 -= 32 // $t1 save the address of the [OldProc] address
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
    ld $sp, (8 * 8)($sp)
    ld $s0, (9 * 8)($sp)   #
    ld $s1, (10 * 8)($sp)   #
    ld $s2, (11 * 8)($sp)   #
    ld $s3, (12 * 8)($sp)   #
    ld $s4, (13 * 8)($sp)   #
    ld $s5, (14 * 8)($sp)   #
    ld $s6, (15 * 8)($sp)   #
    ld $s7, (16 * 8)($sp)   #
    ld $s8, (17 * 8)($sp)   #
    daddiu  $sp, $sp, 256

    # -------------------- #

    jr $12                  # jr $t0

call_hook_handler:
## adjust return address
#lea %rax, [%rip + call_net_outro]
#mov %r9, [%rsp + 32 + 8 * 16 + 6 * 8 + 8] ## %r9 = original %rsp
#mov qword ptr [%r9], %rax

    # get pc instruction
    bal 4
    move $14,$31            # move t2,ra
#daddiu $13, $14, -12    # t1 = t2 - 12 // $t1 save the base line, 12 is offset from base line to PC
    daddiu $ra, $14, 32    # t1 = t2 - 12 // $t1 save the base line, 12 is offset from base line to PC
#ld $ra, call_net_outro

    # get Hook Proc Address and saved into $t9 register
    ld $13, (6 * 8)($sp)    # restore base line to t1 
    daddiu $13, $13, -24    # t1 -= 24 // $t1 save the address of the [NewProc] address
    ld $12, 0($13)          # ld t0, t1 // read 8 byte from $t1, t0 save the [NewProc] address
    move $25, $12           # save [NewProc] to $t9, because [$gp] calculate with $t9 in next stack

    j trampoline_exit

call_net_outro: ## this is where the handler returns...

    # get arg1
    ld $a0, (6 * 8)($sp)    # a0 save restore base line

    # get arg2
    daddiu $a1, $sp, 256    # a1 save the origin rsp

    # get [NETOutro] address
    ld $13, (6 * 8)($sp)    # restore base line to $t1 
    daddiu $13, $13, -16    # t1 -= 16 // $t1 save the address of the [NETOutro] address
    ld $12, 0($13)          # ld t0, t1 // read 8 byte from $t1, t0 save the [NETOutro] address

    # call Hook->NETOutro(Hook, InitialRSP)
    move $t9, $12
    jalr $t9

    # return
    ld $ra, (0 * 8)($sp)
    ld $a0, (1 * 8)($sp)
    ld $a1, (2 * 8)($sp)
    ld $a2, (3 * 8)($sp)
    ld $a3, (4 * 8)($sp)
    ld $a4, (5 * 8)($sp)
    ld $t9, (7 * 8)($sp)
    ld $s0, (9 * 8)($sp)   #
    ld $s1, (10 * 8)($sp)   #
    ld $s2, (11 * 8)($sp)   #
    ld $s3, (12 * 8)($sp)   #
    ld $s4, (13 * 8)($sp)   #
    ld $s5, (14 * 8)($sp)   #
    ld $s6, (15 * 8)($sp)   #
    ld $s7, (16 * 8)($sp)   #
    ld $s8, (17 * 8)($sp)   #
    ld $t3, (18 * 8)($sp)   # import!!!, $ra will override this region
    daddiu  $sp, $sp, 256
    sd $t3, (0)($sp)
    jr $ra

###################################################################################### generic outro for both cases...
trampoline_exit:

        # -------------------- #

        # restore the registers
#ld $ra, (0 * 8)($sp)
        ld $a0, (1 * 8)($sp)
        ld $a1, (2 * 8)($sp)
        ld $a2, (3 * 8)($sp)
        ld $a3, (4 * 8)($sp)
        ld $a4, (5 * 8)($sp)
        ld $t9, (7 * 8)($sp)
    ld $s0, (9 * 8)($sp)   #
    ld $s1, (10 * 8)($sp)   #
    ld $s2, (11 * 8)($sp)   #
    ld $s3, (12 * 8)($sp)   #
    ld $s4, (13 * 8)($sp)   #
    ld $s5, (14 * 8)($sp)   #
    ld $s6, (15 * 8)($sp)   #
    ld $s7, (16 * 8)($sp)   #
    ld $s8, (17 * 8)($sp)   #
#daddiu  $sp, $sp, 256

        # -------------------- #

        # jump to the address
        move $25, $12           # move $t9, $t0
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

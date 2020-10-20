#include "detours.h"

#if defined(DETOURS_mips)
__asm__
(R"(.intel_syntax
.globl Trampoline_ASM_mips
.globl trampoline_template_mips
.globl trampoline_data_mips

Trampoline_ASM_mips:

#.data:, data segment
NETIntro:
    .qword 0
OldProc:
    .qword 0
NewProc:
    .qword 0
NETOutro:
    .qword 0
IsExecutedPtr:
    .qword 0

trampoline_template_mips:

    addiu $sp,$sp,(-8)
    sd $sp,$sp(0)

    addiu $sp,$sp,(-8)
    sd $sp,$sp(0)

    #andi $sp,$sp,0xFFFFFFFFFFFFFFF0

    addiu $sp,$sp,(-6*8)
    sd (0 * 16)($sp),$a0
    sd (1 * 16)($sp),$a1
    sd (2 * 16)($sp),$a2
    sd (3 * 16)($sp),$a3
    sd (4 * 16)($sp),$t8
    sd (5 * 16)($sp),$t9

    addiu $sp,$sp,(-8*16)
    sd (0 * 16)($sp),$t0
    sd (1 * 16)($sp),$t1
    sd (2 * 16)($sp),$t2
    sd (3 * 16)($sp),$t3
    sd (4 * 16)($sp),$t4
    sd (5 * 16)($sp),$t5
    sd (6 * 16)($sp),$t6
    sd (7 * 16)($sp),$t7

    addiu $sp,$sp,(-32)

    move t8, IsExecutedPtr(pc)
    ld t8, (t8)

## is a user handler available?
    bne NewProc(rip), 0, call_net_entry

###################################################################################### call original method
        move t8, OldProc(pc)
        j trampoline_exit

###################################################################################### call hook handler or original method...
call_net_entry:


## call NET intro
    mov a0, (IsExecutedPtr + 8)(pc)
    mov a1, (32 + 8 * 16 + 6 * 8 + 8)(sp)
    mov a1, (a1)

## should call original method?
    bne v0, 0, call_hook_handler

        move t8, OldProc(pc)
        j trampoline_exit

call_hook_handler:
## adjust return address
    move t8, call_net_outro(pc)
    move t9, (32 + 8 * 16 + 6 * 8 + 8)(sp)
    sd t8, t9

## call hook handler
    move t8, OldProc(pc)
    j trampoline_exit

call_net_outro: ## this is where the handler returns...

## call NET outro
    ## Here we are NOT under the alignment trick.

    addiu $sp,$sp,(-64)
    move t8, (IsExecutedPtr + 8)(rip)
    move t9, 56(sp)
    jal NETOutro(pc)

    ld t9, 56(sp)
    addiu $sp,$sp,(64)

## finally return to saved return address - the caller of this trampoline...
    jr ra

######################################################################################## generic outro for both cases...
trampoline_exit:

    sd (0 * 16)($sp),$t0
    sd (1 * 16)($sp),$t1
    sd (2 * 16)($sp),$t2
    sd (3 * 16)($sp),$t3
    sd (4 * 16)($sp),$t4
    sd (5 * 16)($sp),$t5
    sd (6 * 16)($sp),$t6
    sd (7 * 16)($sp),$t7
    addiu $sp,$sp,(16 * 8)

    sd (0 * 16)($sp),$a0
    sd (1 * 16)($sp),$a1
    sd (2 * 16)($sp),$a2
    sd (3 * 16)($sp),$a3
    sd (4 * 16)($sp),$t8
    sd (5 * 16)($sp),$t9
    addiu $sp,$sp,(16*6)

    ld sp, 8(sp)
    j (t8)

## outro signature, to automatically determine code size

trampoline_data_mips:
    .byte 0x78
    .byte 0x56
    .byte 0x34
    .byte 0x12

)");

#endif

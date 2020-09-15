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

#if 1
    addiu $sp,$sp,(-8)
    sd $sp,$sp(0)

    addiu $sp,$sp,(-8)
    sd $sp,$sp(0)

    #andi $sp,$sp,0xFFFFFFFFFFFFFFF0
#else
    push rsp
    push qword ptr [rsp]
    and rsp, 0xFFFFFFFFFFFFFFF0 # reset the low 4 bit to zero, why ?
#endif

#if 0
    ## rdi，rsi，rdx，rcx，r8，r9 用作函数参数，依次对应第1参数，第2参数。。。
    mov rax, rsp ## rax was assigned by before call trampoline_template_mips? guess rax was zero
    push rdi
    push rsi
    push rdx
    push rcx
    push r8
    push r9
#else
    addiu $sp,$sp,(-6*8)
    sd (0 * 16)($sp),$a0
    sd (1 * 16)($sp),$a1
    sd (2 * 16)($sp),$a2
    sd (3 * 16)($sp),$a3
    sd (4 * 16)($sp),$t8
    sd (5 * 16)($sp),$t9
#endif

#if 0
    sub rsp, 8 * 16 ## space for SSE registers
#else
    addiu $sp,$sp,(-8*16)
#if 0
    ## clear the registers xmm0 ~ xmm7? may be use the invalid(not init) value assign to the registers
    ## [rsp + n * 16] is the non-init space
    movups [rsp + 7 * 16], xmm0
    movups [rsp + 6 * 16], xmm1
    movups [rsp + 5 * 16], xmm2
    movups [rsp + 4 * 16], xmm3
    movups [rsp + 3 * 16], xmm4
    movups [rsp + 2 * 16], xmm5
    movups [rsp + 1 * 16], xmm6
    movups [rsp + 0 * 16], xmm7
#else
    sd (0 * 16)($sp),$t0
    sd (1 * 16)($sp),$t1
    sd (2 * 16)($sp),$t2
    sd (3 * 16)($sp),$t3
    sd (4 * 16)($sp),$t4
    sd (5 * 16)($sp),$t5
    sd (6 * 16)($sp),$t6
    sd (7 * 16)($sp),$t7
#endif

#if 0
    sub rsp, 32## shadow space for method calls
#else
    addiu $sp,$sp,(-32)
#endif

#if 0
    lea rax, [rip + IsExecutedPtr]
    mov rax, [rax]
    .byte 0xF0 ## interlocked increment execution counter
    inc qword ptr [rax] ## rax jump to the [IsExecutedPtr] ? or jump the rip + 8 address
#else
    move t8, IsExecutedPtr(pc)
    ld t8, (t8)
#endif

## is a user handler available?
#if 0
    cmp qword ptr [rip + NewProc], 0 ## NewProc was assigned by outside?

    .byte 0x3E ## branch usually taken
    jne call_net_entry
#else
    bne NewProc(rip), 0, call_net_entry
#endif

###################################################################################### call original method
#if 0
        lea rax, [rip + IsExecutedPtr]
        mov rax, [rax]
        .byte 0xF0 ## interlocked decrement execution counter
        dec qword ptr [rax]

        lea rax, [rip + OldProc]
        jmp trampoline_exit
#else
        move t8, OldProc(pc)
        j trampoline_exit
#endif

###################################################################################### call hook handler or original method...
call_net_entry:


## call NET intro
#if 0
    lea rdi, [rip + IsExecutedPtr + 8] ## Hook handle (only a position hint)
    ## Here we are under the alignment trick.
    mov rdx, [rsp + 32 + 8 * 16 + 6 * 8 + 8] ## rdx = original rsp (address of return address)
    mov rsi, [rdx] ## return address (value stored in original rsp)
    call qword ptr [rip + NETIntro] ## Hook->NETIntro(Hook, RetAddr, InitialRSP)##
#else
    mov a0, (IsExecutedPtr + 8)(pc)
    mov a1, (32 + 8 * 16 + 6 * 8 + 8)(sp)
    mov a1, (a1)
#endif

## should call original method?
#if 0
    test rax, rax

    .byte 0x3E ## branch usually taken
    jne call_hook_handler
#else
    bne v0, 0, call_hook_handler
#endif

#if 0
    ## call original method
        lea rax, [rip + IsExecutedPtr]
        mov rax, [rax]
        .byte 0xF0 ## interlocked decrement execution counter
        dec qword ptr [rax]

        lea rax, [rip + OldProc]
        jmp trampoline_exit
#else
        move t8, OldProc(pc)
        j trampoline_exit
#endif

call_hook_handler:
## adjust return address
#if 0
    lea rax, [rip + call_net_outro]
    ## Here we are under the alignment trick.
    mov r9, [rsp + 32 + 8 * 16 + 6 * 8 + 8] ## r9 = original rsp
    mov qword ptr [r9], rax
#else
    move t8, call_net_outro(pc)
    move t9, (32 + 8 * 16 + 6 * 8 + 8)(sp)
    sd t8, t9
#endif

## call hook handler
#if 0
    lea rax, [rip + NewProc]
    jmp trampoline_exit
#else
    move t8, OldProc(pc)
    j trampoline_exit
#endif

call_net_outro: ## this is where the handler returns...

## call NET outro
    ## Here we are NOT under the alignment trick.

#if 0
    push 0 ## space for return address
    push rax

    sub rsp, 32 + 16## shadow space for method calls and SSE registers
    movups [rsp + 32], xmm0

    lea rdi, [rip + IsExecutedPtr + 8]  ## Param 1: Hook handle hint
    lea rsi, [rsp + 56] ## Param 2: Address of return address
    call qword ptr [rip + NETOutro] ## Hook->NETOutro(Hook)##

    lea rax, [rip + IsExecutedPtr]
    mov rax, [rax]
    .byte 0xF0 ## interlocked decrement execution counter
    dec qword ptr [rax]

    add rsp, 32 + 16
    movups xmm0, [rsp - 16]

    pop rax ## restore return value of user handler...
#else
    addiu $sp,$sp,(-64)
    move t8, (IsExecutedPtr + 8)(rip)
    move t9, 56(sp)
    jal NETOutro(pc)

    ld t9, 56(sp)
    addiu $sp,$sp,(64)
#endif

## finally return to saved return address - the caller of this trampoline...
#if 0
    ret
#else
    jr ra
#endif

######################################################################################## generic outro for both cases...
trampoline_exit:

#if 0
    add rsp, 32 + 16 * 8
    movups xmm7, [rsp - 8 * 16]
    movups xmm6, [rsp - 7 * 16]
    movups xmm5, [rsp - 6 * 16]
    movups xmm4, [rsp - 5 * 16]
    movups xmm3, [rsp - 4 * 16]
    movups xmm2, [rsp - 3 * 16]
    movups xmm1, [rsp - 2 * 16]
    movups xmm0, [rsp - 1 * 16]

    pop r9
    pop r8
    pop rcx
    pop rdx
    pop rsi
    pop rdi

    ## Remove alignment trick: https://stackoverflow.com/a/9600102
    mov rsp, [rsp + 8]

    jmp qword ptr [rax] ## ATTENTION: In case of hook handler we will return to call_net_outro, otherwise to the caller...
#else

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
#endif

## outro signature, to automatically determine code size

trampoline_data_mips:
    .byte 0x78
    .byte 0x56
    .byte 0x34
    .byte 0x12

)");

#endif

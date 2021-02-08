//////////////////////////////////////////////////////////////////////////////
//
//  Core Detours Functionality
//
//

#define _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE 1
#include <boost/format.hpp>
#include "types.h"
#include "limits.h"
#include "plthook.h"
#include <iostream>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <assert.h>
#include <skyguard/LinuxDetours/logger/Logger.h>
#include <skyguard/LinuxDetours/logger/LoggerManager.h>

#define THROW(code, Msg)            { CB_WARN("error code:" << (code) << "error msg:" << (Msg)); goto THROW_OUTRO; }

//#define DETOUR_DEBUG 1
#define DETOURS_INTERNAL

#include "detours.h"

using namespace SGLD_NAMESPACE;

#define NOTHROW

struct _DETOUR_TRAMPOLINE;
#define __4KB_SIZE__         0x1000
#define __256MB_SIZE__       0x10000000
#define __J_INSTRUCTION      0x08000000 // mips64 J inistruction: [6 high bit(0000 10)] + [26 low bit]
#define __IS_OUT_BOUNDARY__(low, val, up) (( (val) < (low) ) || ( (val) > (up) ))

std::string dumpTrampoline(const _DETOUR_TRAMPOLINE* trampoline);
std::string dumpHex(const char* buf, int len)
{
    std::ostringstream oss;
    for (int i = 0; i != len; i++)
    {
        oss << "0x" << std::setw(2) << std::setfill('0') << std::hex << ((int)buf[i] & 0xff) << " ";
        if (((i + 1) % 0x10) == 0)
            oss << std::endl;
    }

    return std::move(oss.str());
}

std::string toMB(uint64_t size)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (float)size / 1024 / 1024 << " MB";
    return oss.str();
}

std::string calculteOffset(uint64_t base, uint64_t lower, uint64_t upper, uint64_t abs_address)
{
    uint64_t offset = 0;
    bool was_blow;
    if ((uint64_t)abs_address < (uint64_t)lower)
    {
        offset = (uint64_t)base - (uint64_t)abs_address;
        was_blow = true;
    }
    else if ((uint64_t)abs_address > (uint64_t)upper)
    {
        offset = (uint64_t)abs_address - (uint64_t)base;
        was_blow = false;
    }

    std::ostringstream oss;
    oss << (was_blow ? "-" : "+") << (toMB(offset));

    return oss.str();
}

//////////////////////////////////////////////////////////////////////////////
//
struct _DETOUR_ALIGN
{
    BYTE    obTarget : 3;
    BYTE    obTrampoline : 5;
};

//C_ASSERT(sizeof(_DETOUR_ALIGN) == 1);

//////////////////////////////////////////////////////////////////////////////
//
// Region reserved for system DLLs, which cannot be used for trampolines.
//
static PVOID    s_pSystemRegionLowerBound = (PVOID)(ULONG_PTR)0x70000000;
static PVOID    s_pSystemRegionUpperBound = (PVOID)(ULONG_PTR)0x80000000;


//////////////////////////////////////////////////////////////////////////////
//
// Hook Handle Slot List
//
ULONG                       GlobalSlotList[MAX_HOOK_COUNT];
static LONG                 UniqueIDCounter = 0x10000000;

//////////////////////////////////////////////////////////////////////////////
//
int detour_get_page_size()
{
    return getpagesize();
}
PVOID detour_get_page(PVOID addr)
{
    ULONG_PTR ptr = (ULONG_PTR)addr;
    return (PVOID)(ptr - (ptr % getpagesize()));
}

static bool detour_is_imported(PBYTE pbCode, PBYTE pbAddress)
{
    plthook_t *plthook;
    plthook_open(&plthook, NULL);
    unsigned int pos = 0; /* This must be initialized with zero. */
    const char *name;
    void **addr;

    while (plthook_enum(plthook, &pos, &name, &addr) == 0) {
        if (addr != NULL && (PBYTE)addr == pbAddress) {
            plthook_close(plthook);
            return true;
        }
    }
    plthook_close(plthook);
    return false;
}

inline ULONG_PTR detour_2gb_below(ULONG_PTR address)
{
    return (address > (ULONG_PTR)0x7ff80000) ? address - 0x7ff80000 : 0x80000;
}

inline ULONG_PTR detour_2gb_above(ULONG_PTR address)
{
#if defined(DETOURS_64BIT)
    return (address < (ULONG_PTR)0xffffffff80000000) ? address + 0x7ff80000 : (ULONG_PTR)0xfffffffffff80000;
#else
    return (address < (ULONG_PTR)0x80000000) ? address + 0x7ff80000 : (ULONG_PTR)0xfff80000;
#endif
}


#ifdef DETOURS_MIPS64
uint64_t byte_alignment_down(uint64_t address, uint64_t alignment_byte)
{
    assert ((alignment_byte % __4KB_SIZE__) == 0);
    return address - (address & (alignment_byte - 1));
}

inline ULONG_PTR find_base_addreess(ULONG_PTR address)
{
    // down to alignment 256MB
    return byte_alignment_down(address, __256MB_SIZE__);
}

inline ULONG_PTR detour_256mb_below(ULONG_PTR address)
{
    return find_base_addreess(address);
}

inline ULONG_PTR detour_256mb_above(ULONG_PTR address)
{
    return find_base_addreess(address) + __256MB_SIZE__;
}
#endif

///////////////////////////////////////////////////////////////////////// X86.
//
#ifdef DETOURS_X86

const ULONG DETOUR_TRAMPOLINE_CODE_SIZE = 128;

struct _DETOUR_TRAMPOLINE
{
    BYTE               rbCode[30];     // target code + jmp to pbRemain
    BYTE               cbCode;         // size of moved target code.
    BYTE               cbCodeBreak;    // padding to make debugging easier.
    BYTE               rbRestore[22];  // original target code.
    BYTE               cbRestore;      // size of original target code.
    BYTE               cbRestoreBreak; // padding to make debugging easier.
    _DETOUR_ALIGN      rAlign[8];      // instruction alignment array.
    PBYTE              pbRemain;       // first instruction after moved code. [free list]
    PBYTE              pbDetour;       // first instruction of detour function.
    HOOK_ACL           LocalACL;
    void*              Callback;
    ULONG              HLSIndex;
    ULONG              HLSIdent;
    TRACED_HOOK_HANDLE OutHandle; // handle returned to user  
    void*              Trampoline;
    INT                IsExecuted;
    void*              HookIntro; // . NET Intro function  
    UCHAR*             OldProc;  // old target function      
    void*              HookProc; // function we detour to
    void*              HookOutro;   // .NET Outro function  
    int*               IsExecutedPtr;
    BYTE               rbTrampolineCode[DETOUR_TRAMPOLINE_CODE_SIZE];
};

C_ASSERT(sizeof(_DETOUR_TRAMPOLINE) == 764);

enum {
    SIZE_OF_JMP = 5
};

inline PBYTE detour_gen_jmp_immediate(PBYTE pbCode, PBYTE pbJmpVal)
{
    PBYTE pbJmpSrc = pbCode + 5;
    *pbCode++ = 0xE9;   // jmp +imm32
    *((INT32*&)pbCode)++ = (INT32)(pbJmpVal - pbJmpSrc);
    return pbCode;
}

inline PBYTE detour_gen_jmp_indirect(PBYTE pbCode, PBYTE *ppbJmpVal)
{
    *pbCode++ = 0xff;   // jmp [+imm32]
    *pbCode++ = 0x25;
    *((INT32*&)pbCode)++ = (INT32)((PBYTE)ppbJmpVal);
    return pbCode;
}

inline PBYTE detour_gen_brk(PBYTE pbCode, PBYTE pbLimit)
{
    while (pbCode < pbLimit) {
        *pbCode++ = 0xcc;   // brk;
    }
    return pbCode;
}

inline PBYTE detour_skip_jmp(PBYTE pbCode, PVOID *ppGlobals)
{
    if (pbCode == NULL) {
        return NULL;
    }
    if (ppGlobals != NULL) {
        *ppGlobals = NULL;
    }

    // First, skip over the import vector if there is one.
    if (pbCode[0] == 0xff && pbCode[1] == 0x25) {   // jmp [imm32]
                                                    // Looks like an import alias jump, then get the code it points to.
        PBYTE pbTarget = *(UNALIGNED PBYTE *)&pbCode[2];
        if (detour_is_imported(pbCode, pbTarget)) {
            PBYTE pbNew = *(UNALIGNED PBYTE *)pbTarget;
            CB_DEBUG((boost::format("%p->%p: skipped over import table.")% pbCode% pbNew).str());
            pbCode = pbNew;
        }
    }

    // Then, skip over a patch jump
    if (pbCode[0] == 0xeb) {   // jmp +imm8
        PBYTE pbNew = pbCode + 2 + *(CHAR *)&pbCode[1];
        CB_DEBUG((boost::format("%p->%p: skipped over short jump.")% pbCode% pbNew).str());
        pbCode = pbNew;

        // First, skip over the import vector if there is one.
        if (pbCode[0] == 0xff && pbCode[1] == 0x25) {   // jmp [imm32]
                                                        // Looks like an import alias jump, then get the code it points to.
            PBYTE pbTarget = *(UNALIGNED PBYTE *)&pbCode[2];
            if (detour_is_imported(pbCode, pbTarget)) {
                pbNew = *(UNALIGNED PBYTE *)pbTarget;
                CB_DEBUG((boost::format("%p->%p: skipped over import table.")% pbCode% pbNew).str());
                pbCode = pbNew;
            }
        }
        // Finally, skip over a long jump if it is the target of the patch jump.
        else if (pbCode[0] == 0xe9) {   // jmp +imm32
            pbNew = pbCode + 5 + *(UNALIGNED INT32 *)&pbCode[1];
            CB_DEBUG((boost::format("%p->%p: skipped over long jump.")% pbCode% pbNew).str());
            pbCode = pbNew;
        }
    }
    return pbCode;
}

inline void detour_find_jmp_bounds(PBYTE pbCode,
    PDETOUR_TRAMPOLINE *ppLower,
    PDETOUR_TRAMPOLINE *ppUpper)
{
    // We have to place trampolines within +/- 2GB of code.
    ULONG_PTR lo = detour_2gb_below((ULONG_PTR)pbCode);
    ULONG_PTR hi = detour_2gb_above((ULONG_PTR)pbCode);
    CB_DEBUG((boost::format("[%p..%p..%p]") % lo % (void*)pbCode % hi).str());

    // And, within +/- 2GB of relative jmp targets.
    if (pbCode[0] == 0xe9) {   // jmp +imm32
        PBYTE pbNew = pbCode + 5 + *(UNALIGNED INT32 *)&pbCode[1];

        if (pbNew < pbCode) {
            hi = detour_2gb_above((ULONG_PTR)pbNew);
        }
        else {
            lo = detour_2gb_below((ULONG_PTR)pbNew);
        }
        CB_DEBUG((boost::format("[%p..%p..%p] +imm32") % lo % (void*)pbCode % hi).str());
    }

    *ppLower = (PDETOUR_TRAMPOLINE)lo;
    *ppUpper = (PDETOUR_TRAMPOLINE)hi;
}

inline BOOL detour_does_code_end_function(PBYTE pbCode)
{
    if (pbCode[0] == 0xeb ||    // jmp +imm8
        pbCode[0] == 0xe9 ||    // jmp +imm32
        pbCode[0] == 0xe0 ||    // jmp eax
        pbCode[0] == 0xc2 ||    // ret +imm8
        pbCode[0] == 0xc3 ||    // ret
        pbCode[0] == 0xcc) {    // brk
        return TRUE;
    }
    else if (pbCode[0] == 0xf3 && pbCode[1] == 0xc3) {  // rep ret
        return TRUE;
    }
    else if (pbCode[0] == 0xff && pbCode[1] == 0x25) {  // jmp [+imm32]
        return TRUE;
    }
    else if ((pbCode[0] == 0x26 ||      // jmp es:
        pbCode[0] == 0x2e ||      // jmp cs:
        pbCode[0] == 0x36 ||      // jmp ss:
        pbCode[0] == 0x3e ||      // jmp ds:
        pbCode[0] == 0x64 ||      // jmp fs:
        pbCode[0] == 0x65) &&     // jmp gs:
        pbCode[1] == 0xff &&       // jmp [+imm32]
        pbCode[2] == 0x25) {
        return TRUE;
    }
    return FALSE;
}

inline ULONG detour_is_code_filler(PBYTE pbCode)
{
    // 1-byte through 11-byte NOPs.
    if (pbCode[0] == 0x90) {
        return 1;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x90) {
        return 2;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x00) {
        return 3;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x40 &&
        pbCode[3] == 0x00) {
        return 4;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x44 &&
        pbCode[3] == 0x00 && pbCode[4] == 0x00) {
        return 5;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x0F && pbCode[2] == 0x1F &&
        pbCode[3] == 0x44 && pbCode[4] == 0x00 && pbCode[5] == 0x00) {
        return 6;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x80 &&
        pbCode[3] == 0x00 && pbCode[4] == 0x00 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00) {
        return 7;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x84 &&
        pbCode[3] == 0x00 && pbCode[4] == 0x00 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00) {
        return 8;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x0F && pbCode[2] == 0x1F &&
        pbCode[3] == 0x84 && pbCode[4] == 0x00 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00 && pbCode[8] == 0x00) {
        return 9;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x66 && pbCode[2] == 0x0F &&
        pbCode[3] == 0x1F && pbCode[4] == 0x84 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00 && pbCode[8] == 0x00 &&
        pbCode[9] == 0x00) {
        return 10;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x66 && pbCode[2] == 0x66 &&
        pbCode[3] == 0x0F && pbCode[4] == 0x1F && pbCode[5] == 0x84 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00 && pbCode[8] == 0x00 &&
        pbCode[9] == 0x00 && pbCode[10] == 0x00) {
        return 11;
    }

    // int 3.
    if (pbCode[0] == 0xcc) {
        return 1;
    }
    return 0;
}

#endif // DETOURS_X86

///////////////////////////////////////////////////////////////////////// X64.
//
#ifdef DETOURS_X64

const ULONG DETOUR_TRAMPOLINE_CODE_SIZE = 0x150;

struct _DETOUR_TRAMPOLINE
{
    // An X64 instuction can be 15 bytes long.
    // In practice 11 seems to be the limit.
    BYTE               rbCode[30];     // target code + jmp to pbRemain.
    BYTE               cbCode;         // size of moved target code.
    BYTE               cbCodeBreak;    // padding to make debugging easier.
    BYTE               rbRestore[30];  // original target code.
    BYTE               cbRestore;      // size of original target code.
    BYTE               cbRestoreBreak; // padding to make debugging easier.
    _DETOUR_ALIGN      rAlign[8];      // instruction alignment array.
    PBYTE              pbRemain;       // first instruction after moved code. [free list]
    PBYTE              pbDetour;       // first instruction of detour function.
    BYTE               rbCodeIn[8];    // jmp [pbDetour]
    HOOK_ACL           LocalACL;
    void*              Callback;
    ULONG              HLSIndex;
    ULONG              HLSIdent;
    TRACED_HOOK_HANDLE OutHandle; // handle returned to user  
    void*              Trampoline;
    INT                IsExecuted;
    void*              HookIntro; // . NET Intro function  
    UCHAR*             OldProc;  // old target function      
    void*              HookProc; // function we detour to
    void*              HookOutro;   // .NET Outro function  
    int*               IsExecutedPtr;
    BYTE               rbTrampolineCode[DETOUR_TRAMPOLINE_CODE_SIZE];
};

//C_ASSERT(sizeof(_DETOUR_TRAMPOLINE) == 968);

enum {
    SIZE_OF_JMP = 5
};

inline PBYTE detour_gen_jmp_immediate(PBYTE pbCode, PBYTE pbJmpVal)
{
    PBYTE pbJmpSrc = pbCode + 5;
    *pbCode++ = 0xE9;   // jmp +imm32
    *((INT32*&)pbCode)++ = (INT32)(pbJmpVal - pbJmpSrc);

    int instruction_len = 5;
    PBYTE pbCodeOrigin = pbCode - instruction_len;
    CB_DEBUG((boost::format("detour_gen_jmp_immediate() "
                "pbJmpVal: %p, pbJmpSrc: %p (%p + %d), offset: %x, "
                "jmp code(%p ): %s")
                % (void*)pbJmpVal % (void*)pbJmpSrc % (void*)pbCodeOrigin % instruction_len % (INT32)(pbJmpVal - pbJmpSrc)
                % (void*)pbCodeOrigin % dumpHex((const char*)pbCodeOrigin, instruction_len).c_str()
                ).str());

    return pbCode;
}

inline PBYTE detour_gen_jmp_indirect(PBYTE pbCode, PBYTE *ppbJmpVal)
{
    PBYTE pbJmpSrc = pbCode + 6;
    *pbCode++ = 0xff;   // jmp [+imm32]
    *pbCode++ = 0x25;
    *((INT32*&)pbCode)++ = (INT32)((PBYTE)ppbJmpVal - pbJmpSrc);

    int instruction_len = 6;
    PBYTE pbCodeOrigin = pbCode - instruction_len;
    CB_DEBUG((boost::format("detour_gen_jmp_indirect() "
                "pbJmpVal: (address: %p val: %p), pbJmpSrc: %p (%p + %d), offset: %x, "
                "jmp code(%p ): %s")
                % (PBYTE)ppbJmpVal% *ppbJmpVal% pbJmpSrc% pbCodeOrigin% instruction_len% (INT32)((PBYTE)ppbJmpVal - pbJmpSrc)
                % pbCodeOrigin% dumpHex((const char*)pbCodeOrigin% instruction_len).c_str()
                ).str());

    return pbCode;
}

inline PBYTE detour_gen_brk(PBYTE pbCode, PBYTE pbLimit)
{
    while (pbCode < pbLimit) {
        *pbCode++ = 0xcc;   // brk;
    }
    return pbCode;
}

inline PBYTE detour_skip_jmp(PBYTE pbCode, PVOID *ppGlobals)
{
    if (pbCode == NULL) {
        return NULL;
    }
    if (ppGlobals != NULL) {
        *ppGlobals = NULL;
    }

    // First, skip over the import vector if there is one.
    if (pbCode[0] == 0xff && pbCode[1] == 0x25) {   // jmp [+imm32]
                                                    // Looks like an import alias jump, then get the code it points to.
        PBYTE pbTarget = pbCode + 6 + *(UNALIGNED INT32 *)&pbCode[2];
        if (detour_is_imported(pbCode, pbTarget)) {
            PBYTE pbNew = *(UNALIGNED PBYTE *)pbTarget;
            CB_DEBUG((boost::format("%p->%p: skipped over import table")% pbCode% pbNew).str());
            pbCode = pbNew;
        }
    }

    // Then, skip over a patch jump
    if (pbCode[0] == 0xeb) {   // jmp +imm8
        PBYTE pbNew = pbCode + 2 + *(CHAR *)&pbCode[1];
        CB_DEBUG((boost::format("%p->%p: skipped over short jump")% pbCode% pbNew).str());
        pbCode = pbNew;

        // First, skip over the import vector if there is one.
        if (pbCode[0] == 0xff && pbCode[1] == 0x25) {   // jmp [+imm32]
                                                        // Looks like an import alias jump, then get the code it points to.
            PBYTE pbTarget = pbCode + 6 + *(UNALIGNED INT32 *)&pbCode[2];
            if (detour_is_imported(pbCode, pbTarget)) {
                pbNew = *(UNALIGNED PBYTE *)pbTarget;
                CB_DEBUG((boost::format("%p->%p: skipped over import table.")% pbCode% pbNew).str());
                pbCode = pbNew;
            }
        }
        // Finally, skip over a long jump if it is the target of the patch jump.
        else if (pbCode[0] == 0xe9) {   // jmp +imm32
            pbNew = pbCode + 5 + *(UNALIGNED INT32 *)&pbCode[1];
            CB_DEBUG((boost::format("%p->%p: skipped over long jump.")% pbCode% pbNew).str());
            pbCode = pbNew;
        }
    }
    return pbCode;
}

inline void detour_find_jmp_bounds(PBYTE pbCode,
    PDETOUR_TRAMPOLINE *ppLower,
    PDETOUR_TRAMPOLINE *ppUpper)
{
    // We have to place trampolines within +/- 2GB of code.
    ULONG_PTR lo = detour_2gb_below((ULONG_PTR)pbCode);
    ULONG_PTR hi = detour_2gb_above((ULONG_PTR)pbCode);
    CB_DEBUG((boost::format("[%p..%p..%p]")% lo% (void*)pbCode% hi).str());

    // And, within +/- 2GB of relative jmp vectors.
    if (pbCode[0] == 0xff && pbCode[1] == 0x25) {   // jmp [+imm32]
        PBYTE pbNew = pbCode + 6 + *(UNALIGNED INT32 *)&pbCode[2];

        if (pbNew < pbCode) {
            hi = detour_2gb_above((ULONG_PTR)pbNew);
        }
        else {
            lo = detour_2gb_below((ULONG_PTR)pbNew);
        }
        CB_DEBUG((boost::format("[%p..%p..%p] [+imm32]")% lo% (void*)pbCode% hi).str());
    }
    // And, within +/- 2GB of relative jmp targets.
    else if (pbCode[0] == 0xe9) {   // jmp +imm32
        PBYTE pbNew = pbCode + 5 + *(UNALIGNED INT32 *)&pbCode[1];

        if (pbNew < pbCode) {
            hi = detour_2gb_above((ULONG_PTR)pbNew);
        }
        else {
            lo = detour_2gb_below((ULONG_PTR)pbNew);
        }
        CB_DEBUG((boost::format("[%p..%p..%p] +imm32")% lo% (void*)pbCode% hi).str());
    }

    *ppLower = (PDETOUR_TRAMPOLINE)lo;
    *ppUpper = (PDETOUR_TRAMPOLINE)hi;
}

inline BOOL detour_does_code_end_function(PBYTE pbCode)
{
    if (pbCode[0] == 0xeb ||    // jmp +imm8
        pbCode[0] == 0xe9 ||    // jmp +imm32
        pbCode[0] == 0xe0 ||    // jmp eax
        pbCode[0] == 0xc2 ||    // ret +imm8
        pbCode[0] == 0xc3 ||    // ret
        pbCode[0] == 0xcc) {    // brk
        return TRUE;
    }
    else if (pbCode[0] == 0xf3 && pbCode[1] == 0xc3) {  // rep ret
        return TRUE;
    }
    else if (pbCode[0] == 0xff && pbCode[1] == 0x25) {  // jmp [+imm32]
        return TRUE;
    }
    else if ((pbCode[0] == 0x26 ||      // jmp es:
        pbCode[0] == 0x2e ||      // jmp cs:
        pbCode[0] == 0x36 ||      // jmp ss:
        pbCode[0] == 0x3e ||      // jmp ds:
        pbCode[0] == 0x64 ||      // jmp fs:
        pbCode[0] == 0x65) &&     // jmp gs:
        pbCode[1] == 0xff &&       // jmp [+imm32]
        pbCode[2] == 0x25) {
        return TRUE;
    }
    return FALSE;
}

inline ULONG detour_is_code_filler(PBYTE pbCode)
{
    // 1-byte through 11-byte NOPs.
    if (pbCode[0] == 0x90) {
        return 1;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x90) {
        return 2;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x00) {
        return 3;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x40 &&
        pbCode[3] == 0x00) {
        return 4;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x44 &&
        pbCode[3] == 0x00 && pbCode[4] == 0x00) {
        return 5;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x0F && pbCode[2] == 0x1F &&
        pbCode[3] == 0x44 && pbCode[4] == 0x00 && pbCode[5] == 0x00) {
        return 6;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x80 &&
        pbCode[3] == 0x00 && pbCode[4] == 0x00 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00) {
        return 7;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x84 &&
        pbCode[3] == 0x00 && pbCode[4] == 0x00 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00) {
        return 8;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x0F && pbCode[2] == 0x1F &&
        pbCode[3] == 0x84 && pbCode[4] == 0x00 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00 && pbCode[8] == 0x00) {
        return 9;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x66 && pbCode[2] == 0x0F &&
        pbCode[3] == 0x1F && pbCode[4] == 0x84 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00 && pbCode[8] == 0x00 &&
        pbCode[9] == 0x00) {
        return 10;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x66 && pbCode[2] == 0x66 &&
        pbCode[3] == 0x0F && pbCode[4] == 0x1F && pbCode[5] == 0x84 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00 && pbCode[8] == 0x00 &&
        pbCode[9] == 0x00 && pbCode[10] == 0x00) {
        return 11;
    }

    // int 3.
    if (pbCode[0] == 0xcc) {
        return 1;
    }
    return 0;
}

#endif // DETOURS_X64

///////////////////////////////////////////////////////////////////////// mips64.
//
#ifdef DETOURS_MIPS64

const ULONG DETOUR_TRAMPOLINE_CODE_SIZE = 0x400;

struct _DETOUR_TRAMPOLINE
{
    BYTE               rbCode[30];     // target code + jmp to pbRemain.
    BYTE               cbCode;         // size of moved target code.
    BYTE               cbCodeBreak;    // padding to make debugging easier.
    BYTE               rbRestore[30];  // original target code.
    BYTE               cbRestore;      // size of original target code.
    BYTE               cbRestoreBreak; // padding to make debugging easier.
    _DETOUR_ALIGN      rAlign[8];      // instruction alignment array.
    PBYTE              pbRemain;       // first instruction after moved code. [free list]
    PBYTE              pbDetour;       // first instruction of detour function.
    BYTE               rbCodeIn[8];    // jmp [pbDetour]
    HOOK_ACL           LocalACL;
    void*              Callback;
    ULONG              HLSIndex;
    ULONG              HLSIdent;
    TRACED_HOOK_HANDLE OutHandle; // handle returned to user
    void*              Trampoline;
    INT                IsExecuted;
    void*              HookIntro; // . NET Intro function
    UCHAR*             OldProc;  // old target function
    void*              HookProc; // function we detour to
    void*              HookOutro;   // .NET Outro function
    int*               IsExecutedPtr;
    BYTE               rbTrampolineCode[DETOUR_TRAMPOLINE_CODE_SIZE];
};

//C_ASSERT(sizeof(_DETOUR_TRAMPOLINE) == 968);

enum {
    SIZE_OF_JMP = 4
};

inline PBYTE detour_gen_jr_ra_inc(PBYTE pbCode)
{
    uint8_t jr_ra_inc[] = {0x32, 0x00, 0x00, 0x08}; // 03e00008 jr ra
    int jr_ra_inc_len = sizeof(jr_ra_inc);
    memcpy(pbCode, jr_ra_inc, jr_ra_inc_len);
    pbCode += jr_ra_inc_len;
}

inline PBYTE detour_gen_jmp_immediate(PBYTE pbCode, PBYTE pbJmpVal)
{
    // The low 28 bits of the target address is the instr_index field shifted left 2bits
    uint32_t instr_index = ((PtrToUlong(pbJmpVal) & 0xfffffff) >> 2);
    uint32_t j_code = __J_INSTRUCTION; // J: 6bit, 000010
    uint32_t j_target_code = j_code | instr_index;

    CB_DEBUG((boost::format("detour_gen_jmp_immediate()"
                "jmp address: %p"
                "instr_index: 0x%08x((%p & 0x0fffffff) >> 2)"
                "j_code: 0x%08X ([6 high bit](0000 10) + [26 low bit])"
                "J OP code: 0x%08x( 0x%08x | 0x%08x )")
                % (void*)pbJmpVal
                % instr_index % (void*)pbJmpVal
                % j_code
                % j_target_code % j_code % instr_index
                ).str());

    memcpy(pbCode, &j_target_code, sizeof(uint32_t));
    pbCode += sizeof(uint32_t);
}

inline std::string generate_asm_hard_code()
{
    uint32_t array[DETOUR_TRAMPOLINE_CODE_SIZE / sizeof(uint32_t)] = {0};
    int i = 0;

    // this is a split comment
    array[i++] = 0x03e0782d;   // 28:    03e0782d    move    t3,ra    
    array[i++] = 0x04110001;   // 2c:    04110001    bal    34    <trampoline_template_mips64+0xc>    
    array[i++] = 0x00000000;   // 30:    00000000    nop    
    array[i++] = 0x03e0702d;   // 34:    03e0702d    move    t2,ra    
    array[i++] = 0x01e0f82d;   // 38:    01e0f82d    move    ra,t3    
    array[i++] = 0x65cdfff4;   // 3c:    65cdfff4    daddiu    t1,t2,-12    
    array[i++] = 0xdfaf0000;   // 40:    dfaf0000    ld    t3,0(sp)    
    array[i++] = 0x67bdfe00;   // 44:    67bdfe00    daddiu    sp,sp,-512    
    array[i++] = 0xffbf0000;   // 48:    ffbf0000    sd    ra,0(sp)    
    array[i++] = 0xffa40008;   // 4c:    ffa40008    sd    a0,8(sp)    
    array[i++] = 0xffa50010;   // 50:    ffa50010    sd    a1,16(sp)    
    array[i++] = 0xffa60018;   // 54:    ffa60018    sd    a2,24(sp)    
    array[i++] = 0xffa70020;   // 58:    ffa70020    sd    a3,32(sp)    
    array[i++] = 0xffa80028;   // 5c:    ffa80028    sd    a4,40(sp)    
    array[i++] = 0xffad0030;   // 60:    ffad0030    sd    t1,48(sp)    
    array[i++] = 0xffb90038;   // 64:    ffb90038    sd    t9,56(sp)    
    array[i++] = 0xffbd0040;   // 68:    ffbd0040    sd    sp,64(sp)    
    array[i++] = 0xffb00048;   // 6c:    ffb00048    sd    s0,72(sp)    
    array[i++] = 0xffb10050;   // 70:    ffb10050    sd    s1,80(sp)    
    array[i++] = 0xffb20058;   // 74:    ffb20058    sd    s2,88(sp)    
    array[i++] = 0xffb30060;   // 78:    ffb30060    sd    s3,96(sp)    
    array[i++] = 0xffb40068;   // 7c:    ffb40068    sd    s4,104(sp)    
    array[i++] = 0xffb50070;   // 80:    ffb50070    sd    s5,112(sp)    
    array[i++] = 0xffb60078;   // 84:    ffb60078    sd    s6,120(sp)    
    array[i++] = 0xffb70080;   // 88:    ffb70080    sd    s7,128(sp)    
    array[i++] = 0xffbe0088;   // 8c:    ffbe0088    sd    s8,136(sp)    
    array[i++] = 0xffaf0090;   // 90:    ffaf0090    sd    t3,144(sp)    
    array[i++] = 0xffa90098;   // 94:    ffa90098    sd    a5,152(sp)    
    array[i++] = 0xffaa00a0;   // 98:    ffaa00a0    sd    a6,160(sp)    
    array[i++] = 0xffab00a8;   // 9c:    ffab00a8    sd    a7,168(sp)    
    array[i++] = 0xffa200f0;   // a0:    ffa200f0    sd    v0,240(sp)    
    array[i++] = 0xffa300f8;   // a4:    ffa300f8    sd    v1,248(sp)    
    array[i++] = 0xdfa40030;   // a8:    dfa40030    ld    a0,48(sp)    
    array[i++] = 0x03e0282d;   // ac:    03e0282d    move    a1,ra    
    array[i++] = 0x67a60200;   // b0:    67a60200    daddiu    a2,sp,512    
    array[i++] = 0xdfad0030;   // b4:    dfad0030    ld    t1,48(sp)    
    array[i++] = 0x65adffd8;   // b8:    65adffd8    daddiu    t1,t1,-40    
    array[i++] = 0xddac0000;   // bc:    ddac0000    ld    t0,0(t1)    
    array[i++] = 0x0180c82d;   // c0:    0180c82d    move    t9,t0    
    array[i++] = 0x0320f809;   // c4:    0320f809    jalr    t9    
    array[i++] = 0x00000000;   // c8:    00000000    nop    
    array[i++] = 0x1440001d;   // cc:    1440001d    bnez    v0,144    <call_hook_handler>    
    array[i++] = 0x00000000;   // d0:    00000000    nop    
    array[i++] = 0xdfad0030;   // d4:    dfad0030    ld    t1,48(sp)    
    array[i++] = 0x65adffe0;   // d8:    65adffe0    daddiu    t1,t1,-32    
    array[i++] = 0xddac0000;   // dc:    ddac0000    ld    t0,0(t1)    
    array[i++] = 0xdfbf0000;   // e0:    dfbf0000    ld    ra,0(sp)    
    array[i++] = 0xdfa40008;   // e4:    dfa40008    ld    a0,8(sp)    
    array[i++] = 0xdfa50010;   // e8:    dfa50010    ld    a1,16(sp)    
    array[i++] = 0xdfa60018;   // ec:    dfa60018    ld    a2,24(sp)    
    array[i++] = 0xdfa70020;   // f0:    dfa70020    ld    a3,32(sp)    
    array[i++] = 0xdfa80028;   // f4:    dfa80028    ld    a4,40(sp)    
    array[i++] = 0xdfb90038;   // f8:    dfb90038    ld    t9,56(sp)    
    array[i++] = 0xdfbd0040;   // fc:    dfbd0040    ld    sp,64(sp)    
    array[i++] = 0xdfb00048;   // 100:    dfb00048    ld    s0,72(sp)    
    array[i++] = 0xdfb10050;   // 104:    dfb10050    ld    s1,80(sp)    
    array[i++] = 0xdfb20058;   // 108:    dfb20058    ld    s2,88(sp)    
    array[i++] = 0xdfb30060;   // 10c:    dfb30060    ld    s3,96(sp)    
    array[i++] = 0xdfb40068;   // 110:    dfb40068    ld    s4,104(sp)    
    array[i++] = 0xdfb50070;   // 114:    dfb50070    ld    s5,112(sp)    
    array[i++] = 0xdfb60078;   // 118:    dfb60078    ld    s6,120(sp)    
    array[i++] = 0xdfb70080;   // 11c:    dfb70080    ld    s7,128(sp)    
    array[i++] = 0xdfbe0088;   // 120:    dfbe0088    ld    s8,136(sp)    
    array[i++] = 0xdfa90098;   // 124:    dfa90098    ld    a5,152(sp)    
    array[i++] = 0xdfaa00a0;   // 128:    dfaa00a0    ld    a6,160(sp)    
    array[i++] = 0xdfab00a8;   // 12c:    dfab00a8    ld    a7,168(sp)    
    array[i++] = 0xdfa200f0;   // 130:    dfa200f0    ld    v0,240(sp)    
    array[i++] = 0xdfa300f8;   // 134:    dfa300f8    ld    v1,248(sp)    
    array[i++] = 0x67bd0200;   // 138:    67bd0200    daddiu    sp,sp,512    
    array[i++] = 0x01800008;   // 13c:    01800008    jr    t0    
    array[i++] = 0x00000000;   // 140:    00000000    nop    
    array[i++] = 0x04110001;   // 144:    04110001    bal    14c    <call_hook_handler+0x8>    
    array[i++] = 0x00000000;   // 148:    00000000    nop    
    array[i++] = 0x03e0702d;   // 14c:    03e0702d    move    t2,ra    
    array[i++] = 0x65df0020;   // 150:    65df0020    daddiu    ra,t2,32    
    array[i++] = 0xdfad0030;   // 154:    dfad0030    ld    t1,48(sp)    
    array[i++] = 0x65adffe8;   // 158:    65adffe8    daddiu    t1,t1,-24    
    array[i++] = 0xddac0000;   // 15c:    ddac0000    ld    t0,0(t1)    
    array[i++] = 0x0180c82d;   // 160:    0180c82d    move    t9,t0    
    array[i++] = 0x10000025;   // 164:    10000025    b    1fc    <trampoline_exit>    
    array[i++] = 0x00000000;   // 168:    00000000    nop    
    array[i++] = 0xffa20140;   // 16c:    ffa20140    sd    v0,320(sp)    
    array[i++] = 0xffa30148;   // 170:    ffa30148    sd    v1,328(sp)    
    array[i++] = 0xdfa40030;   // 174:    dfa40030    ld    a0,48(sp)    
    array[i++] = 0x67a50200;   // 178:    67a50200    daddiu    a1,sp,512    
    array[i++] = 0xdfad0030;   // 17c:    dfad0030    ld    t1,48(sp)    
    array[i++] = 0x65adfff0;   // 180:    65adfff0    daddiu    t1,t1,-16    
    array[i++] = 0xddac0000;   // 184:    ddac0000    ld    t0,0(t1)    
    array[i++] = 0x0180c82d;   // 188:    0180c82d    move    t9,t0    
    array[i++] = 0x0320f809;   // 18c:    0320f809    jalr    t9    
    array[i++] = 0x00000000;   // 190:    00000000    nop    
    array[i++] = 0xdfbf0000;   // 194:    dfbf0000    ld    ra,0(sp)    
    array[i++] = 0xdfa40008;   // 198:    dfa40008    ld    a0,8(sp)    
    array[i++] = 0xdfa50010;   // 19c:    dfa50010    ld    a1,16(sp)    
    array[i++] = 0xdfa60018;   // 1a0:    dfa60018    ld    a2,24(sp)    
    array[i++] = 0xdfa70020;   // 1a4:    dfa70020    ld    a3,32(sp)    
    array[i++] = 0xdfa80028;   // 1a8:    dfa80028    ld    a4,40(sp)    
    array[i++] = 0xdfb90038;   // 1ac:    dfb90038    ld    t9,56(sp)    
    array[i++] = 0xdfb00048;   // 1b0:    dfb00048    ld    s0,72(sp)    
    array[i++] = 0xdfb10050;   // 1b4:    dfb10050    ld    s1,80(sp)    
    array[i++] = 0xdfb20058;   // 1b8:    dfb20058    ld    s2,88(sp)    
    array[i++] = 0xdfb30060;   // 1bc:    dfb30060    ld    s3,96(sp)    
    array[i++] = 0xdfb40068;   // 1c0:    dfb40068    ld    s4,104(sp)    
    array[i++] = 0xdfb50070;   // 1c4:    dfb50070    ld    s5,112(sp)    
    array[i++] = 0xdfb60078;   // 1c8:    dfb60078    ld    s6,120(sp)    
    array[i++] = 0xdfb70080;   // 1cc:    dfb70080    ld    s7,128(sp)    
    array[i++] = 0xdfbe0088;   // 1d0:    dfbe0088    ld    s8,136(sp)    
    array[i++] = 0xdfaf0090;   // 1d4:    dfaf0090    ld    t3,144(sp)    
    array[i++] = 0xdfa90098;   // 1d8:    dfa90098    ld    a5,152(sp)    
    array[i++] = 0xdfaa00a0;   // 1dc:    dfaa00a0    ld    a6,160(sp)    
    array[i++] = 0xdfab00a8;   // 1e0:    dfab00a8    ld    a7,168(sp)    
    array[i++] = 0xdfa20140;   // 1e4:    dfa20140    ld    v0,320(sp)    
    array[i++] = 0xdfa30148;   // 1e8:    dfa30148    ld    v1,328(sp)    
    array[i++] = 0x67bd0200;   // 1ec:    67bd0200    daddiu    sp,sp,512    
    array[i++] = 0xffaf0000;   // 1f0:    ffaf0000    sd    t3,0(sp)    
    array[i++] = 0x03e00008;   // 1f4:    03e00008    jr    ra    
    array[i++] = 0x00000000;   // 1f8:    00000000    nop    
    array[i++] = 0xdfa40008;   // 1fc:    dfa40008    ld    a0,8(sp)    
    array[i++] = 0xdfa50010;   // 200:    dfa50010    ld    a1,16(sp)    
    array[i++] = 0xdfa60018;   // 204:    dfa60018    ld    a2,24(sp)    
    array[i++] = 0xdfa70020;   // 208:    dfa70020    ld    a3,32(sp)    
    array[i++] = 0xdfa80028;   // 20c:    dfa80028    ld    a4,40(sp)    
    array[i++] = 0xdfb90038;   // 210:    dfb90038    ld    t9,56(sp)    
    array[i++] = 0xdfb00048;   // 214:    dfb00048    ld    s0,72(sp)    
    array[i++] = 0xdfb10050;   // 218:    dfb10050    ld    s1,80(sp)    
    array[i++] = 0xdfb20058;   // 21c:    dfb20058    ld    s2,88(sp)    
    array[i++] = 0xdfb30060;   // 220:    dfb30060    ld    s3,96(sp)    
    array[i++] = 0xdfb40068;   // 224:    dfb40068    ld    s4,104(sp)    
    array[i++] = 0xdfb50070;   // 228:    dfb50070    ld    s5,112(sp)    
    array[i++] = 0xdfb60078;   // 22c:    dfb60078    ld    s6,120(sp)    
    array[i++] = 0xdfb70080;   // 230:    dfb70080    ld    s7,128(sp)    
    array[i++] = 0xdfbe0088;   // 234:    dfbe0088    ld    s8,136(sp)    
    array[i++] = 0xdfa90098;   // 238:    dfa90098    ld    a5,152(sp)    
    array[i++] = 0xdfaa00a0;   // 23c:    dfaa00a0    ld    a6,160(sp)    
    array[i++] = 0xdfab00a8;   // 240:    dfab00a8    ld    a7,168(sp)    
    array[i++] = 0xdfa200f0;   // 244:    dfa200f0    ld    v0,240(sp)    
    array[i++] = 0xdfa300f8;   // 248:    dfa300f8    ld    v1,248(sp)    
    array[i++] = 0x0180c82d;   // 24c:    0180c82d    move    t9,t0    
    array[i++] = 0x03200008;   // 250:    03200008    jr    t9    
    array[i++] = 0x00000000;   // 254:    00000000    nop    
    array[i++] = 0x12345678;   // 258:    12345678    beq    s1,s4,15c3c    <SEGMENT1+0x159e0>    
    array[i++] = 0x64636261;   // 25c:    64636261    daddiu    v1,v1,25185    
    array[i++] = 0x00676665;   // 260:    00676665    0x676665    
    // this is a split comment

    std::string hard_code(reinterpret_cast<char*>(array), sizeof(array));
    return hard_code;
}

inline PBYTE detour_gen_jmp_indirect(PBYTE pbCode, PBYTE *ppbJmpVal)
{
    PBYTE pbJmpSrc = pbCode + 6;
    *pbCode++ = 0xff;   // jmp [+imm32]
    *pbCode++ = 0x25;
    *((INT32*&)pbCode)++ = (INT32)((PBYTE)ppbJmpVal - pbJmpSrc);
    return pbCode;
}

inline PBYTE detour_gen_brk(PBYTE pbCode, PBYTE pbLimit)
{
    while (pbCode < pbLimit) {
#if 0
        *pbCode++ = 0xcc;   // brk;
#else
        *pbCode++ = 0x00;   // brk;
#endif
    }
    return pbCode;
}

inline PBYTE detour_skip_jmp(PBYTE pbCode, PVOID *ppGlobals)
{
    if (pbCode == NULL) {
        return NULL;
    }
    if (ppGlobals != NULL) {
        *ppGlobals = NULL;
    }

    // First, skip over the import vector if there is one.
    if (pbCode[0] == 0xff && pbCode[1] == 0x25) {   // jmp [+imm32]
                                                    // Looks like an import alias jump, then get the code it points to.
        PBYTE pbTarget = pbCode + 6 + *(UNALIGNED INT32 *)&pbCode[2];
        if (detour_is_imported(pbCode, pbTarget)) {
            PBYTE pbNew = *(UNALIGNED PBYTE *)pbTarget;
            CB_DEBUG((boost::format("%p->%p: skipped over import table.")% pbCode% pbNew).str());
            pbCode = pbNew;
        }
    }

    // Then, skip over a patch jump
    if (pbCode[0] == 0xeb) {   // jmp +imm8
        PBYTE pbNew = pbCode + 2 + *(CHAR *)&pbCode[1];
        CB_DEBUG((boost::format("%p->%p: skipped over short jump.")% pbCode% pbNew).str());
        pbCode = pbNew;

        // First, skip over the import vector if there is one.
        if (pbCode[0] == 0xff && pbCode[1] == 0x25) {   // jmp [+imm32]
                                                        // Looks like an import alias jump, then get the code it points to.
            PBYTE pbTarget = pbCode + 6 + *(UNALIGNED INT32 *)&pbCode[2];
            if (detour_is_imported(pbCode, pbTarget)) {
                pbNew = *(UNALIGNED PBYTE *)pbTarget;
                CB_DEBUG((boost::format("%p->%p: skipped over import table.")% pbCode% pbNew).str());
                pbCode = pbNew;
            }
        }
        // Finally, skip over a long jump if it is the target of the patch jump.
        else if (pbCode[0] == 0xe9) {   // jmp +imm32
            pbNew = pbCode + 5 + *(UNALIGNED INT32 *)&pbCode[1];
            CB_DEBUG((boost::format("%p->%p: skipped over long jump.")% pbCode% pbNew).str());
            pbCode = pbNew;
        }
    }
    return pbCode;
}

inline void detour_find_jmp_bounds(PBYTE pbCode,
    PDETOUR_TRAMPOLINE *ppLower,
    PDETOUR_TRAMPOLINE *ppUpper)
{
    // We have to place trampolines within +/- 256MB of code.
    ULONG_PTR lo = detour_256mb_below((ULONG_PTR)pbCode);
    ULONG_PTR hi = detour_256mb_above((ULONG_PTR)pbCode);
    CB_DEBUG((boost::format("[%p(-256MB)..%p..%p(+256MB)]")% lo% (void*)pbCode% hi).str());

    *ppLower = (PDETOUR_TRAMPOLINE)lo;
    *ppUpper = (PDETOUR_TRAMPOLINE)hi;
}

inline BOOL detour_does_code_end_function(PBYTE pbCode)
{
    if (pbCode[0] == 0xeb ||    // jmp +imm8
        pbCode[0] == 0xe9 ||    // jmp +imm32
        pbCode[0] == 0xe0 ||    // jmp eax
        pbCode[0] == 0xc2 ||    // ret +imm8
        pbCode[0] == 0xc3 ||    // ret
        pbCode[0] == 0xcc) {    // brk
        return TRUE;
    }
    else if (pbCode[0] == 0xf3 && pbCode[1] == 0xc3) {  // rep ret
        return TRUE;
    }
    else if (pbCode[0] == 0xff && pbCode[1] == 0x25) {  // jmp [+imm32]
        return TRUE;
    }
    else if ((pbCode[0] == 0x26 ||      // jmp es:
        pbCode[0] == 0x2e ||      // jmp cs:
        pbCode[0] == 0x36 ||      // jmp ss:
        pbCode[0] == 0x3e ||      // jmp ds:
        pbCode[0] == 0x64 ||      // jmp fs:
        pbCode[0] == 0x65) &&     // jmp gs:
        pbCode[1] == 0xff &&       // jmp [+imm32]
        pbCode[2] == 0x25) {
        return TRUE;
    }
    return FALSE;
}

inline ULONG detour_is_code_filler(PBYTE pbCode)
{
    // 1-byte through 11-byte NOPs.
    if (pbCode[0] == 0x90) {
        return 1;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x90) {
        return 2;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x00) {
        return 3;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x40 &&
        pbCode[3] == 0x00) {
        return 4;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x44 &&
        pbCode[3] == 0x00 && pbCode[4] == 0x00) {
        return 5;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x0F && pbCode[2] == 0x1F &&
        pbCode[3] == 0x44 && pbCode[4] == 0x00 && pbCode[5] == 0x00) {
        return 6;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x80 &&
        pbCode[3] == 0x00 && pbCode[4] == 0x00 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00) {
        return 7;
    }
    if (pbCode[0] == 0x0F && pbCode[1] == 0x1F && pbCode[2] == 0x84 &&
        pbCode[3] == 0x00 && pbCode[4] == 0x00 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00) {
        return 8;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x0F && pbCode[2] == 0x1F &&
        pbCode[3] == 0x84 && pbCode[4] == 0x00 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00 && pbCode[8] == 0x00) {
        return 9;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x66 && pbCode[2] == 0x0F &&
        pbCode[3] == 0x1F && pbCode[4] == 0x84 && pbCode[5] == 0x00 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00 && pbCode[8] == 0x00 &&
        pbCode[9] == 0x00) {
        return 10;
    }
    if (pbCode[0] == 0x66 && pbCode[1] == 0x66 && pbCode[2] == 0x66 &&
        pbCode[3] == 0x0F && pbCode[4] == 0x1F && pbCode[5] == 0x84 &&
        pbCode[6] == 0x00 && pbCode[7] == 0x00 && pbCode[8] == 0x00 &&
        pbCode[9] == 0x00 && pbCode[10] == 0x00) {
        return 11;
    }

    // int 3.
    if (pbCode[0] == 0xcc) {
        return 1;
    }
    return 0;
}

#endif // DETOURS_MIPS64

//////////////////////////////////////////////////////////////////////// IA64.
//
#ifdef DETOURS_IA64


#endif // DETOURS_IA64

#ifdef DETOURS_ARM

const ULONG DETOUR_TRAMPOLINE_CODE_SIZE = 0x110;


struct _DETOUR_TRAMPOLINE
{
    // A Thumb-2 instruction can be 2 or 4 bytes long.
    BYTE               rbCode[62];     // target code + jmp to pbRemain
    BYTE               cbCode;         // size of moved target code.
    BYTE               cbCodeBreak;    // padding to make debugging easier.
    BYTE               rbRestore[22];  // original target code.
    BYTE               cbRestore;      // size of original target code.
    BYTE               cbRestoreBreak; // padding to make debugging easier.
    _DETOUR_ALIGN      rAlign[8];      // instruction alignment array.
    PBYTE              pbRemain;       // first instruction after moved code. [free list]
    PBYTE              pbDetour;       // first instruction of detour function.
    INT                IsThumbTarget;
    HOOK_ACL           LocalACL;
    void*              Callback;
    ULONG              HLSIndex;
    ULONG              HLSIdent;
    TRACED_HOOK_HANDLE OutHandle; // handle returned to user  
    void*              Trampoline;
    INT                IsExecuted;
    void*              HookIntro; // . NET Intro function  
    UCHAR*             OldProc;  // old target function      
    void*              HookProc; // function we detour to
    void*              HookOutro;   // .NET Outro function  
    int*               IsExecutedPtr;
    BYTE               rbTrampolineCode[DETOUR_TRAMPOLINE_CODE_SIZE];
};

//C_ASSERT(sizeof(_DETOUR_TRAMPOLINE) == 900);

enum {
    SIZE_OF_JMP = 8
};

inline PBYTE align4(PBYTE pValue)
{
    return (PBYTE)(((ULONG)pValue) & ~(ULONG)3u);
}
inline ULONG fetch_opcode(PBYTE pbCode)
{
    ULONG Opcode = *(UINT32 *)&pbCode[0];
    return Opcode;
}
inline ULONG fetch_thumb_opcode(PBYTE pbCode)
{
    ULONG Opcode = *(UINT16 *)&pbCode[0];
    if (Opcode >= 0xe800) {
        Opcode = (Opcode << 16) | *(UINT16 *)&pbCode[2];
    }
    return Opcode;
}

inline void write_thumb_opcode(PBYTE &pbCode, ULONG Opcode)
{
    if (Opcode >= 0x10000) {
        *((UINT16*&)pbCode)++ = Opcode >> 16;
    }
    *((UINT16*&)pbCode)++ = (UINT16)Opcode;
}
inline void write_arm_opcode(PBYTE &pbCode, ULONG Opcode)
{
    *((UINT32*&)pbCode)++ = (UINT32)Opcode;
}
#define A$ldr_rd_$rn_im$(rd, rn, im) /* ldr rd, [rn, #im] */ \
    (0xe5100000 | ((im) < 0 ? 0 : 1 << 23) | ((rn) << 16) | ((rd) << 12) | abs(im))

PBYTE detour_gen_jmp_immediate(PBYTE pbCode, PBYTE *ppPool, PBYTE pbJmpVal)
{
#if defined(DETOURS_ARM32)
    PBYTE pbLiteral;
    if (ppPool != NULL) {
        *ppPool = *ppPool - 4;
        pbLiteral = *ppPool;
    }
    else {
        pbLiteral = align4(pbCode + 4);
    }
    *((PBYTE*&)pbLiteral) = pbJmpVal;
    LONG delta = pbLiteral - align4(pbCode + 4);
    // stored as: F0 04 1F E5 
    *((UINT32*&)pbCode)++ = A$ldr_rd_$rn_im$(15, 15, delta - 4);
    if (ppPool == NULL) {
        if (((ULONG)pbCode & 2) != 0) {
            write_arm_opcode(pbCode, 0xe320f000);
        }
        pbCode += 4;
    }
    //*((UINT32*&)pbCode)++ = (UINT32)0xF004E51F | (delta);
#elif defined(DETOURS_ARM)
    if (reinterpret_cast<uintptr_t>(pbCode) & 0x1) {
        // reset is_thumb_flag
        pbCode = DETOURS_PFUNC_TO_PBYTE(pbCode);
        PBYTE pbLiteral;
        if (ppPool != NULL) {
            *ppPool = *ppPool - 4;
            pbLiteral = *ppPool;
        }
        else {
            pbLiteral = align4(pbCode + 6);
        }
        *((PBYTE*&)pbLiteral) = DETOURS_PBYTE_TO_PFUNC(pbJmpVal);
        LONG delta = pbLiteral - align4(pbCode + 4);

        // stored as: DF F8 00 F0 
        write_thumb_opcode(pbCode, 0xf8dff000 | delta);     // LDR PC,[PC+n]
        //write_thumb_opcode(pbCode, 0x9FE504f0 | delta);  

        //write_thumb_opcode(pbCode, 0xF000DFF8 | delta);

        if (ppPool == NULL) {
            if (((ULONG)pbCode & 2) != 0) {
                write_thumb_opcode(pbCode, 0xdefe);         // BREAK
            }
            pbCode += 4;
        }
    }
    else {
        PBYTE pbLiteral;
        if (ppPool != NULL) {
            *ppPool = *ppPool - 4;
            pbLiteral = *ppPool;
        }
        else {
            pbLiteral = align4(pbCode + 4);
        }
        *((PBYTE*&)pbLiteral) = pbJmpVal;
        LONG delta = pbLiteral - align4(pbCode + 4);
        // stored as: F0 04 1F E5 
        *((UINT32*&)pbCode)++ = A$ldr_rd_$rn_im$(15, 15, delta - 4);
        if (ppPool == NULL) {
            if (((ULONG)pbCode & 2) != 0) {
                write_arm_opcode(pbCode, 0xe320f000);
            }
            pbCode += 4;
        }
    }
#endif
    return pbCode;
}

inline PBYTE detour_gen_brk(PBYTE pbCode, PBYTE pbLimit)
{
    while (pbCode < pbLimit) {
#if defined(DETOURS_ARM32)
        write_arm_opcode(pbCode, 0xe320f000);
#elif defined(DETOURS_ARM)
        write_thumb_opcode(pbCode, 0xdefe);
#endif
    }
    return pbCode;
}

inline PBYTE detour_skip_jmp(PBYTE pbCode, PVOID *ppGlobals)
{
    if (pbCode == NULL) {
        return NULL;
    }
    ULONG * isThumb = NULL;
    if (ppGlobals != NULL) {
        isThumb = (ULONG *)*ppGlobals;
        *ppGlobals = NULL;
    }

    if (isThumb != nullptr && *isThumb == 1) {
        // read Thumb instruction set

        // Skip over the import jump if there is one.
        pbCode = (PBYTE)DETOURS_PFUNC_TO_PBYTE(pbCode);
        ULONG Opcode = fetch_thumb_opcode(pbCode);

        if ((Opcode & 0xfbf08f00) == 0xf2400c00) {          // movw r12,#xxxx
            ULONG Opcode2 = fetch_thumb_opcode(pbCode + 4);

            if ((Opcode2 & 0xfbf08f00) == 0xf2c00c00) {      // movt r12,#xxxx
                ULONG Opcode3 = fetch_thumb_opcode(pbCode + 8);
                if (Opcode3 == 0xf8dcf000) {                 // ldr  pc,[r12]
                    PBYTE pbTarget = (PBYTE)(((Opcode2 << 12) & 0xf7000000) |
                        ((Opcode2 << 1) & 0x08000000) |
                        ((Opcode2 << 16) & 0x00ff0000) |
                        ((Opcode >> 4) & 0x0000f700) |
                        ((Opcode >> 15) & 0x00000800) |
                        ((Opcode >> 0) & 0x000000ff));

                    if (detour_is_imported(pbCode, pbTarget)) {
                        PBYTE pbNew = *(PBYTE *)pbTarget;
                        pbNew = DETOURS_PFUNC_TO_PBYTE(pbNew);
                        CB_DEBUG((boost::format("%p->%p: skipped over import table.")% pbCode% pbNew).str());
                        return pbNew;
                    }
                }
            }
        }
    }
    else {
        // read ARM instruction set
        // Skip over the import jump if there is one.
        
        ULONG Opcode = fetch_opcode(pbCode);

        if ((Opcode & 0xe28f0000) == 0xe28f0000) {          // adr r12, #xxxx
            ULONG Opcode2 = fetch_opcode(pbCode + 4);
            if ((Opcode2 & 0xe28c0000) == 0xe28c0000) {      // add r12, r12, #xxxx
                ULONG Opcode3 = fetch_opcode(pbCode + 8);
                if ((Opcode3 & 0xe5bcf000) == 0xe5bcf000) {             // ldr  pc,[r12]
                    ULONG target = (Opcode2 << 12) & 0x000FFFFF;
                    PBYTE pbTarget = /*(PBYTE)(((Opcode2 << 12) & 0xf7000000) |
                                     ((Opcode2 << 1) & 0x08000000) |
                                     ((Opcode2 << 16) & 0x00ff0000) |
                                     ((Opcode >> 4) & 0x0000f700) |
                                     ((Opcode >> 15) & 0x00000800) |
                                     ((Opcode >> 0) & 0x000000ff)); */

                                     //pbTarget = (PBYTE)(*(ULONG*)((pbCode + 8 + tgt + (Opcode3 & 0xFFF))));
                         ((pbCode + 8 + target + (Opcode3 & 0xFFF)));
                    if (detour_is_imported(pbCode, pbTarget)) {
                        PBYTE pbNew = *(PBYTE *)pbTarget;
                        pbNew = DETOURS_PFUNC_TO_PBYTE(pbNew);
                        CB_DEBUG((boost::format("%p->%p: skipped over import table.")% pbCode% pbNew).str());
                        return pbNew;
                    }
                }
            }
        }
    }

    return pbCode;
}

inline void detour_find_jmp_bounds(PBYTE pbCode,
    PDETOUR_TRAMPOLINE *ppLower,
    PDETOUR_TRAMPOLINE *ppUpper)
{
    // We have to place trampolines within +/- 2GB of code.
    ULONG_PTR lo = detour_2gb_below((ULONG_PTR)pbCode);
    ULONG_PTR hi = detour_2gb_above((ULONG_PTR)pbCode);
    CB_DEBUG((boost::format("[%p..%p..%p]")% lo% (void*)pbCode% hi).str());

    *ppLower = (PDETOUR_TRAMPOLINE)lo;
    *ppUpper = (PDETOUR_TRAMPOLINE)hi;
}


inline BOOL detour_does_code_end_function(PBYTE pbCode)
{
    ULONG Opcode = fetch_thumb_opcode(pbCode);
    if ((Opcode & 0xffffff87) == 0x4700 ||          // bx <reg>
        (Opcode & 0xf800d000) == 0xf0009000) {      // b <imm20>
        return TRUE;
    }
    if ((Opcode & 0xffff8000) == 0xe8bd8000) {      // pop {...,pc}
        __debugbreak();
        return TRUE;
    }
    if ((Opcode & 0xffffff00) == 0x0000bd00) {      // pop {...,pc}
        __debugbreak();
        return TRUE;
    }
    return FALSE;
}

inline ULONG detour_is_code_filler(PBYTE pbCode)
{
    if (pbCode[0] == 0x00 && pbCode[1] == 0xbf) { // nop.
        return 2;
    }
    if (pbCode[0] == 0x00 && pbCode[1] == 0x00) { // zero-filled padding.
        return 2;
    }
    return 0;
}

#endif // DETOURS_ARM

#ifdef DETOURS_ARM64

// must be aligned by 8
const ULONG DETOUR_TRAMPOLINE_CODE_SIZE = 0x158;

struct _DETOUR_TRAMPOLINE
{
    // Src: https://github.com/Microsoft/Detours/commit/c5cb6c3af5a6871df47131d6cc29d4262a412623
    // An ARM64 instruction is 4 bytes long.
    // The overwrite is always 2 instructions plus a literal, so 16 bytes, 4 instructions.
    //
    // Copied instructions can expand.
    //
    // The scheme using MovImmediate can cause an instruction
    // to grow as much as 6 times.
    // That would be Bcc or Tbz with a large address space:
    //   4 instructions to form immediate
    //   inverted tbz/bcc
    //   br
    //
    // An expansion of 4 is not uncommon -- bl/blr and small address space:
    //   3 instructions to form immediate
    //   br or brl
    //
    // A theoretical maximum for rbCode is thefore 4*4*6 + 16 = 112 (another 16 for jmp to pbRemain).
    //
    // With literals, the maximum expansion is 5, including the literals: 4*4*5 + 16 = 96.
    //
    // The number is rounded up to 128. m_rbScratchDst should match this.
    //
    BYTE               rbCode[128];     // target code + jmp to pbRemain
    BYTE               cbCode;         // size of moved target code.
    BYTE               cbCodeBreak[3]; // padding to make debugging easier.
    BYTE               rbRestore[24];  // original target code.
    BYTE               cbRestore;      // size of original target code.
    BYTE               cbRestoreBreak[3]; // padding to make debugging easier.
    _DETOUR_ALIGN      rAlign[8];      // instruction alignment array.
    PBYTE              pbRemain;       // first instruction after moved code. [free list]
    PBYTE              pbDetour;       // first instruction of detour function.
    HOOK_ACL           LocalACL;
    void*              Callback;
    ULONG              HLSIndex;
    ULONG              HLSIdent;
    TRACED_HOOK_HANDLE OutHandle; // handle returned to user  
    void*              Trampoline;
    void*              HookIntro; // . NET Intro function  
    UCHAR*             OldProc;  // old target function      
    void*              HookProc; // function we detour to
    void*              HookOutro;   // .NET Outro function  
    int*               IsExecutedPtr;
    BYTE               rbTrampolineCode[DETOUR_TRAMPOLINE_CODE_SIZE];
};

//C_ASSERT(sizeof(_DETOUR_TRAMPOLINE) == 1128);

enum {
    SIZE_OF_JMP = 16
};

inline ULONG fetch_opcode(PBYTE pbCode)
{
    return *(ULONG *)pbCode;
}

inline void write_opcode(PBYTE &pbCode, ULONG Opcode)
{
    *(ULONG *)pbCode = Opcode;
    pbCode += 4;
}

PBYTE detour_gen_jmp_immediate(PBYTE pbCode, PBYTE *ppPool, PBYTE pbJmpVal)
{
    PBYTE pbLiteral;
    if (ppPool != NULL) {
        *ppPool = *ppPool - 8;
        pbLiteral = *ppPool;
    }
    else {
        pbLiteral = pbCode + 2 * 4;
    }

    *((PBYTE*&)pbLiteral) = pbJmpVal;
    LONG delta = (LONG)(pbLiteral - pbCode);

    write_opcode(pbCode, 0x58000011 | ((delta / 4) << 5));  // LDR X17,[PC+n]
    write_opcode(pbCode, 0xd61f0000 | (17 << 5));           // BR X17

    if (ppPool == NULL) {
        pbCode += 8;
    }
    return pbCode;
}

inline PBYTE detour_gen_brk(PBYTE pbCode, PBYTE pbLimit)
{
    while (pbCode < pbLimit) {
        write_opcode(pbCode, 0xd4100000 | (0xf000 << 5));
    }
    return pbCode;
}
inline INT64 detour_sign_extend(UINT64 value, UINT bits)
{
    const UINT left = 64 - bits;
    const INT64 m1 = -1;
    const INT64 wide = (INT64)(value << left);
    const INT64 sign = (wide < 0) ? (m1 << left) : 0;
    return value | sign;
}
inline PBYTE detour_skip_jmp(PBYTE pbCode, PVOID *ppGlobals)
{
    if (pbCode == NULL) {
        return NULL;
    }
    if (ppGlobals != NULL) {
        *ppGlobals = NULL;
    }
    // From here: https://github.com/Microsoft/Detours/pull/8
    // Skip over the import jump if there is one.
    pbCode = (PBYTE)pbCode;
    ULONG Opcode = fetch_opcode(pbCode);
    if ((Opcode & 0x9f00001f) == 0x90000010) {           // adrp  x16, IAT
        ULONG Opcode2 = fetch_opcode(pbCode + 4);

        if ((Opcode2 & 0xffe003fe) == 0xf9400210) {      // ldr   x16, [x16, IAT] | ldr   x17, [x16, IAT]
            ULONG Opcode3 = fetch_opcode(pbCode + 8);

            if ((Opcode3 & 0x91020210) == 0x91020210) {                 // ADD             X16, X16, IAT
                ULONG Opcode4 = fetch_opcode(pbCode + 0xC);
                if ((Opcode4 & 0xd61f0200) == 0xd61f0200) {                 // br    x16 | br x17
                    /* https://static.docs.arm.com/ddi0487/bb/DDI0487B_b_armv8_arm.pdf
                    The ADRP instruction shifts a signed, 21-bit immediate left by 12 bits, adds it to the value of the program counter with
                    the bottom 12 bits cleared to zero, and then writes the result to a general-purpose register. This permits the
                    calculation of the address at a 4KB aligned memory region. In conjunction with an ADD (immediate) instruction, or
                    a Load/Store instruction with a 12-bit immediate offset, this allows for the calculation of, or access to, any address
                    within 4GB of the current PC.
                    PC-rel. addressing
                    This section describes the encoding of the PC-rel. addressing instruction class. The encodings in this section are
                    decoded from Data Processing -- Immediate on page C4-226.
                    Add/subtract (immediate)
                    This section describes the encoding of the Add/subtract (immediate) instruction class. The encodings in this section
                    are decoded from Data Processing -- Immediate on page C4-226.
                    Decode fields
                    Instruction page
                    op
                    0 ADR
                    1 ADRP
                    C6.2.10 ADRP
                    Form PC-relative address to 4KB page adds an immediate value that is shifted left by 12 bits, to the PC value to
                    form a PC-relative address, with the bottom 12 bits masked out, and writes the result to the destination register.
                    ADRP <Xd>, <label>
                    imm = SignExtend(immhi:immlo:Zeros(12), 64);
                    31  30 29 28 27 26 25 24 23 5    4 0
                    1   immlo  1  0  0  0  0  immhi  Rd
                    9             0
                    Rd is hardcoded as 0x10 above.
                    Immediate is 21 signed bits split into 2 bits and 19 bits, and is scaled by 4K.
                    */
                    UINT64 const pageLow2 = (Opcode >> 29) & 3;
                    UINT64 const pageHigh19 = (Opcode >> 5) & ~(~(INT64)0 << 19);
                    INT64 const page = detour_sign_extend((pageHigh19 << 2) | pageLow2, 21) << 12;
                    
                    /* https://static.docs.arm.com/ddi0487/bb/DDI0487B_b_armv8_arm.pdf
                    C6.2.101 LDR (immediate)
                    Load Register (immediate) loads a word or doubleword from memory and writes it to a register. The address that is
                    used for the load is calculated from a base register and an immediate offset.
                    The Unsigned offset variant scales the immediate offset value by the size of the value accessed before adding it
                    to the base register value.
                    Unsigned offset
                    64-bit variant Applies when size == 11.
                    31 30 29 28  27 26 25 24  23 22  21   10   9 5   4 0
                    1  x  1  1   1  0  0  1   0  1  imm12      Rn    Rt
                    F             9        4              200    10
                    That is, two low 5 bit fields are registers, hardcoded as 0x10 and 0x10 << 5 above,
                    then unsigned size-unscaled (8) 12-bit offset, then opcode bits 0xF94.
                    */
                    UINT64 const offset = ((Opcode2 >> 10) & ~(~(INT64)0 << 12)) << 3;
                    
                    PBYTE const pbTarget = (PBYTE)((ULONG64)pbCode & 0xfffffffffffff000ULL) + page + offset;
                    if (detour_is_imported(pbCode, pbTarget)) {
                        PBYTE pbNew = *(PBYTE *)pbTarget;
                        CB_DEBUG((boost::format("%p->%p: skipped over import table.")% pbCode% pbNew).str());
                        return pbNew;
                    }
                }
            }
        }
    }
    return pbCode;
}

inline BOOL detour_does_code_end_function(PBYTE pbCode)
{
    ULONG Opcode = fetch_opcode(pbCode);
    if ((Opcode & 0xfffffc1f) == 0xd65f0000 ||      // br <reg>
        (Opcode & 0xfc000000) == 0x14000000) {      // b <imm26>
        return TRUE;
    }
    return FALSE;
}

inline ULONG detour_is_code_filler(PBYTE pbCode)
{
    if (*(ULONG *)pbCode == 0xd503201f) {   // nop.
        return 4;
    }
    if (*(ULONG *)pbCode == 0x00000000) {   // zero-filled padding.
        return 4;
    }
    return 0;
}
inline void detour_find_jmp_bounds(PBYTE pbCode,
    PDETOUR_TRAMPOLINE *ppLower,
    PDETOUR_TRAMPOLINE *ppUpper)
{
    // We have to place trampolines within +/- 2GB of code.
    ULONG_PTR lo = detour_2gb_below((ULONG_PTR)pbCode);
    ULONG_PTR hi = detour_2gb_above((ULONG_PTR)pbCode);
    CB_DEBUG((boost::format("[%p..%p..%p]")% lo% pbCode% hi).str());

    *ppLower = (PDETOUR_TRAMPOLINE)lo;
    *ppUpper = (PDETOUR_TRAMPOLINE)hi;
}
#endif // DETOURS_ARM64

//////////////////////////////////////////////// Trampoline Memory Management.
//
struct DETOUR_REGION
{
    ULONG               dwSignature;
    DETOUR_REGION *     pNext;  // Next region in list of regions.
    DETOUR_TRAMPOLINE * pFree;  // List of free trampolines in this region.
};
typedef DETOUR_REGION * PDETOUR_REGION;

const ULONG DETOUR_REGION_SIGNATURE = 'Rrtd';
const ULONG DETOUR_REGION_SIZE = 0x2000;
const ULONG DETOUR_TRAMPOLINES_PER_REGION = (DETOUR_REGION_SIZE
    / (sizeof(DETOUR_TRAMPOLINE))) - 1;
static PDETOUR_REGION s_pRegions = NULL;            // List of all regions.
static PDETOUR_REGION s_pRegion = NULL;             // Default region.

static DWORD detour_writable_trampoline_regions()
{
    // Mark all of the regions as writable.
    for (PDETOUR_REGION pRegion = s_pRegions; pRegion != NULL; pRegion = pRegion->pNext) {
        if (mprotect(detour_get_page((PBYTE)pRegion), DETOUR_REGION_SIZE, PAGE_EXECUTE_READWRITE)) {
            // Failed
            return -1;
        }
    }
    return NO_ERROR;
}

static void detour_runnable_trampoline_regions()
{
    // Mark all of the regions as executable.
    for (PDETOUR_REGION pRegion = s_pRegions; pRegion != NULL; pRegion = pRegion->pNext) {
        if (mprotect(detour_get_page((PBYTE)pRegion), DETOUR_REGION_SIZE, PAGE_EXECUTE_READ)) {
            // Failed
        }
    }
}

static PBYTE detour_alloc_round_down_to_region(PBYTE pbTry)
{
    // WinXP64 returns free areas that aren't REGION aligned to 32-bit applications.
    ULONG_PTR extra = ((ULONG_PTR)pbTry) & (DETOUR_REGION_SIZE - 1);
    if (extra != 0) {
        pbTry -= extra;
    }
    return pbTry;
}

static PBYTE detour_alloc_round_up_to_region(PBYTE pbTry)
{
    // WinXP64 returns free areas that aren't REGION aligned to 32-bit applications.
    ULONG_PTR extra = ((ULONG_PTR)pbTry) & (DETOUR_REGION_SIZE - 1);
    if (extra != 0) {
        ULONG_PTR adjust = DETOUR_REGION_SIZE - extra;
        pbTry += adjust;
    }
    return pbTry;
}

// Starting at pbLo, try to allocate a memory region, continue until pbHi.

static PVOID detour_alloc_region_from_lo(PBYTE pbLo, PBYTE pbHi)
{
    PBYTE pbTry = detour_alloc_round_up_to_region(pbLo);

    CB_DEBUG((boost::format(" Looking for free region in %p..%p from %p:")% (void*)pbLo% (void*)pbHi% (void*)pbTry).str());
    for (; pbTry < pbHi;) {
        CB_DEBUG((boost::format("try to found memory [PROT_EXEC | PROT_READ | PROT_WRITE], start address:[%p], size:[0x%x]")
                % pbTry % DETOUR_REGION_SIZE
                ).str());
        PVOID pv = mmap(pbTry, DETOUR_REGION_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (pv != NULL) {
            CB_DEBUG((boost::format("detour_alloc_region_from_lo(), call mmap() ok, Looking for free region successed!!!,"
                    "in %p..%p from %p, mapped area pointer %p")
                    % pbLo % pbHi % pbTry % pv
                    ).str());
            return pv;
        }
        else
        {
            CB_DEBUG((boost::format("detour_alloc_region_from_lo() call mmap() failed, Looking for free region failed!!!, "
                    "in %p..%p from %p")
                    % pbLo% pbHi% pbTry
                    ).str());
        }
        pbTry += DETOUR_REGION_SIZE;

        //else {
        //pbTry = detour_alloc_round_up_to_region((PBYTE)mbi.BaseAddress + mbi.RegionSize);
        //}
    }
    /*
    for (; pbTry < pbHi;) {
    MEMORY_BASIC_INFORMATION mbi;

    if (pbTry >= s_pSystemRegionLowerBound && pbTry <= s_pSystemRegionUpperBound) {
    // Skip region reserved for system DLLs, but preserve address space entropy.
    pbTry += 0x08000000;
    continue;
    }

    ZeroMemory(&mbi, sizeof(mbi));
    if (!VirtualQuery(pbTry, &mbi, sizeof(mbi))) {
    break;
    }

    pbTry,
    mbi.BaseAddress,
    (PBYTE)mbi.BaseAddress + mbi.RegionSize - 1,
    mbi.State));

    if (mbi.State == MEM_FREE && mbi.RegionSize >= DETOUR_REGION_SIZE) {

    PVOID pv = malloc(DETOUR_REGION_SIZE);
    VirtualAlloc(pbTry,
    DETOUR_REGION_SIZE,
    MEM_COMMIT|MEM_RESERVE,
    PAGE_EXECUTE_READWRITE);
    if (pv != NULL) {
    return pv;
    }
    pbTry += DETOUR_REGION_SIZE;
    }
    else {
    pbTry = detour_alloc_round_up_to_region((PBYTE)mbi.BaseAddress + mbi.RegionSize);
    }
    }
    */
    return NULL;
}

// Starting at pbHi, try to allocate a memory region, continue until pbLo.

static PVOID detour_alloc_region_from_hi(PBYTE pbLo, PBYTE pbHi)
{
    PBYTE pbTry = detour_alloc_round_down_to_region(pbHi - DETOUR_REGION_SIZE);
    CB_DEBUG((boost::format(" Looking for free region in %p..%p from %p:")% (void*)pbLo% (void*)pbHi% (void*)pbTry).str());
    //for (; pbTry < pbHi;) {
    for (; pbTry > pbLo;) {
        CB_DEBUG((boost::format("try to found memory [PROT_EXEC | PROT_READ | PROT_WRITE], start address:[%p], size:[0x%x]")
                 % pbTry % DETOUR_REGION_SIZE
                 ).str());
        PVOID pv = mmap(pbTry, DETOUR_REGION_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (pv != NULL) {
            CB_DEBUG((boost::format("detour_alloc_round_down_to_region() call mmap() ok, Looking for free region successed!!!, "
                    "in %p..%p from %p, mapped area pointer %p")
                    % pbLo % pbHi % pbTry % pv
                    ).str());
            return pv;
        }
        else
        {
            CB_DEBUG((boost::format("detour_alloc_round_down_to_region() call mmap() failed, Looking for free region failed!!!, "
                    "in %p..%p from %p")
                    % pbLo % pbHi % pbTry
                    ).str());
        }
        //pbTry += DETOUR_REGION_SIZE; // may be pbTry -= DETOUR_REGION_SIZE?
        pbTry -= DETOUR_REGION_SIZE;
    }

    return NULL;
}

static PVOID detour_alloc_region_in_boundary(PBYTE low, PBYTE high, PBYTE target)
{
    CB_DEBUG((boost::format("Looking for free region in boundary [ %p , %p ] for target [%p]")% (void*)low% (void*)high% (void*)target).str());
    int retry_times = 0;
    int max_retry_times = (__256MB_SIZE__ / DETOUR_REGION_SIZE);
    PBYTE pbTry = low;
    bool found_region = false;
    while (pbTry < high - DETOUR_REGION_SIZE)
    {
        CB_DEBUG((boost::format("try to found memory [PROT_EXEC | PROT_READ | PROT_WRITE], start address:[%p], size:[0x%x]")
                % (void*)pbTry% DETOUR_REGION_SIZE
                ).str());
        PVOID pv = mmap(pbTry, DETOUR_REGION_SIZE, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (pv != NULL)
        {
            CB_DEBUG((boost::format("detour_alloc_round_down_to_region() call mmap() ok, Looking for free region successed, "
                    "in boundary [ %p , %p ] for target [%p], mapped area pointer %p")
                    % (void*)low % (void*)high % (void*)pbTry % (void*)pv
                    ).str());

            if (__IS_OUT_BOUNDARY__((uint64_t)low, (uint64_t)pv, (uint64_t)high))
            {
                CB_DEBUG((boost::format("region alloc successed but trampoline address out of boundary, "
                        "target:%p, trampoline:%p, low:%p, up:%p, "
                        "offset:[%s], "
                        "current retry times:[%d], max retry times:[%d]")
                        % (void*)target % (void*)pv % (void*)low % (void*)high
                        % (calculteOffset((uint64_t)target, (uint64_t)low, (uint64_t)high, (uint64_t)pv)).c_str()
                        % retry_times% max_retry_times
                        ).str());

                if (munmap(pv, DETOUR_REGION_SIZE) == -1)
                {
                    CB_DEBUG((boost::format("detour_alloc_round_down_to_region() call munmap() failed, release free region failed!!!, "
                            "in boundary [ %p , %p ] for target [%p], mapped area pointer %p")
                            % (void*)low % (void*)high % (void*)pbTry % (void*)pv
                            ).str());
                }
            }
            else
            {
                found_region =true;
            }
        }
        else
        {
            CB_DEBUG((boost::format("detour_alloc_round_down_to_region() call mmap() failed, Looking for free region failed!!!, "
                    "in boundary [ %p , %p ] for target [%p], current retry times:[%d]")
                     % (void*)low % (void*)high % (void*)pbTry % retry_times
                     ).str());
        }

        if (found_region)
        {
            CB_DEBUG((boost::format("detour_alloc_round_down_to_region() successed to found free region!!!, "
                    "in boundary [ %p , %p ] for target [%p], mapped area pointer %p")
                     % (void*)low % (void*)high % (void*)pbTry % (void*)pv
                     ).str());
            return pv;
        }

        retry_times++;
        pbTry += DETOUR_REGION_SIZE;
    }

    return NULL;
}

#ifndef DETOURS_MIPS64
PDETOUR_TRAMPOLINE detour_alloc_trampoline(PBYTE pbTarget)
{
    // We have to place trampolines within +/- 2GB of target.

    PDETOUR_TRAMPOLINE pLo;
    PDETOUR_TRAMPOLINE pHi;

    detour_find_jmp_bounds(pbTarget, &pLo, &pHi);

    PDETOUR_TRAMPOLINE pTrampoline = NULL;

    // Insure that there is a default region.
    if (s_pRegion == NULL && s_pRegions != NULL) {
        s_pRegion = s_pRegions;
    }

    // First check the default region for an valid free block.
    if (s_pRegion != NULL && s_pRegion->pFree != NULL &&
        s_pRegion->pFree >= pLo && s_pRegion->pFree <= pHi) {

    found_region:
        pTrampoline = s_pRegion->pFree;
        // do a last sanity check on region.
        if (pTrampoline < pLo || pTrampoline > pHi) {
            return NULL;
        }
        s_pRegion->pFree = (PDETOUR_TRAMPOLINE)pTrampoline->pbRemain;
        memset(pTrampoline, 0xcc, sizeof(*pTrampoline));
        return pTrampoline;
    }

    // Then check the existing regions for a valid free block.
    for (s_pRegion = s_pRegions; s_pRegion != NULL; s_pRegion = s_pRegion->pNext) {
        if (s_pRegion != NULL && s_pRegion->pFree != NULL &&
            s_pRegion->pFree >= pLo && s_pRegion->pFree <= pHi) {
            goto found_region;
        }
    }

    // We need to allocate a new region.

    // Round pbTarget down to 64KB block.
    pbTarget = pbTarget - (PtrToUlong(pbTarget) & 0xffff);

    PVOID pbTry = NULL;

    // NB: We must always also start the search at an offset from pbTarget
    //     in order to maintain ASLR entropy.

#if defined(DETOURS_64BIT)
    // Try looking 1GB below or lower.
    if (pbTry == NULL && pbTarget > (PBYTE)0x40000000) {
        pbTry = detour_alloc_region_from_hi((PBYTE)pLo, pbTarget - 0x40000000);
    }
    // Try looking 1GB above or higher.
    if (pbTry == NULL && pbTarget < (PBYTE)0xffffffff40000000) {
        pbTry = detour_alloc_region_from_lo(pbTarget + 0x40000000, (PBYTE)pHi);
    }
    // Try looking 1GB below or higher.
    if (pbTry == NULL && pbTarget >(PBYTE)0x40000000) {
        pbTry = detour_alloc_region_from_lo(pbTarget - 0x40000000, pbTarget);
    }
    // Try looking 1GB above or lower.
    if (pbTry == NULL && pbTarget < (PBYTE)0xffffffff40000000) {
        pbTry = detour_alloc_region_from_hi(pbTarget, pbTarget + 0x40000000);
    }
#endif

    // Try anything below.
    if (pbTry == NULL) {
        pbTry = detour_alloc_region_from_hi((PBYTE)pLo, pbTarget);
    }
    // try anything above.
    if (pbTry == NULL) {
        pbTry = detour_alloc_region_from_lo(pbTarget, (PBYTE)pHi);
    }

    if (pbTry != NULL) {
        s_pRegion = (DETOUR_REGION*)pbTry;
        s_pRegion->dwSignature = DETOUR_REGION_SIGNATURE;
        s_pRegion->pFree = NULL;
        s_pRegion->pNext = s_pRegions;
        s_pRegions = s_pRegion;
        CB_DEBUG((boost::format("  Allocated region %p..%p")
                    % s_pRegion% ((PBYTE)s_pRegion + DETOUR_REGION_SIZE - 1)
                    ).str());

        // Put everything but the first trampoline on the free list.
        PBYTE pFree = NULL;
        pTrampoline = ((PDETOUR_TRAMPOLINE)s_pRegion) + 1;
        for (int i = DETOUR_TRAMPOLINES_PER_REGION - 1; i > 1; i--) {
            pTrampoline[i].pbRemain = pFree;
            pFree = (PBYTE)&pTrampoline[i];
        }
        s_pRegion->pFree = (PDETOUR_TRAMPOLINE)pFree;
        goto found_region;
    }

    CB_DEBUG(("Couldn't find available memory region!"));
    return NULL;
}
#else
PDETOUR_TRAMPOLINE detour_alloc_trampoline(PBYTE pbTarget) // DETOURS_MIPS64
{
    // We have to place trampolines within +/- 256MB of target.
    CB_DEBUG((boost::format("trigger detour_alloc_trampoline() target address:[%p]") % (void*)pbTarget).str());

    PDETOUR_TRAMPOLINE pLo;
    PDETOUR_TRAMPOLINE pHi;

    CB_DEBUG((boost::format("step 1: found -/+ 256MB(alignment) boundary for address:[%p]") % (void*)pbTarget).str());
    detour_find_jmp_bounds(pbTarget, &pLo, &pHi);

    PDETOUR_TRAMPOLINE pTrampoline = NULL;

    // Insure that there is a default region.
    if (s_pRegion == NULL && s_pRegions != NULL) {
        s_pRegion = s_pRegions;
    }

    // First check the default region for an valid free block.
    if (s_pRegion != NULL && s_pRegion->pFree != NULL &&
        s_pRegion->pFree >= pLo && s_pRegion->pFree <= pHi) {

    found_region:
        pTrampoline = s_pRegion->pFree;
        // do a last sanity check on region.
        if ((uint64_t)pTrampoline < (uint64_t)pLo || (uint64_t)pTrampoline > (uint64_t)pHi) {
            CB_DEBUG((boost::format("region alloc successed but trampoline address out of boundary, "
                    "target:%p, trampoline:%p, low:%p, up:%p, "
                    "offset:[%s]")
                    % (void*)pbTarget % (void*)pTrampoline % (void*)pLo % (void*)pHi
                    % (calculteOffset((uint64_t)pbTarget, (uint64_t)pLo, (uint64_t)pHi, (uint64_t)pTrampoline)).c_str()
                    ).str());
            return NULL;
        }
        s_pRegion->pFree = (PDETOUR_TRAMPOLINE)pTrampoline->pbRemain;
        memset(pTrampoline, 0xcc, sizeof(*pTrampoline));
        return pTrampoline;
    }

    // Then check the existing regions for a valid free block.
    for (s_pRegion = s_pRegions; s_pRegion != NULL; s_pRegion = s_pRegion->pNext) {
        if (s_pRegion != NULL && s_pRegion->pFree != NULL &&
            s_pRegion->pFree >= pLo && s_pRegion->pFree <= pHi) {
            goto found_region;
        }
    }

    // We need to allocate a new region.
    PVOID pbTry = NULL;
    if (pbTry == NULL)
    {
        CB_DEBUG((boost::format("step 2: try to found region from low address(%p) for target(%p)")% (void*)pLo% (void*)pbTarget).str());
        pbTry = detour_alloc_region_in_boundary((PBYTE)pLo, (PBYTE)pHi, pbTarget);
    }

    if (pbTry != NULL) {
        s_pRegion = (DETOUR_REGION*)pbTry;
        s_pRegion->dwSignature = DETOUR_REGION_SIGNATURE;
        s_pRegion->pFree = NULL;
        s_pRegion->pNext = s_pRegions;
        s_pRegions = s_pRegion;
        CB_DEBUG((boost::format("Allocated region %p..%p")
            % (void*)s_pRegion % (void*)((PBYTE)s_pRegion + DETOUR_REGION_SIZE - 1)
            ).str());

        // Put everything but the first trampoline on the free list.
        PBYTE pFree = NULL;
        pTrampoline = ((PDETOUR_TRAMPOLINE)s_pRegion) + 1;
        for (int i = DETOUR_TRAMPOLINES_PER_REGION - 1; i > 1; i--) {
            pTrampoline[i].pbRemain = pFree;
            pFree = (PBYTE)&pTrampoline[i];
        }
        s_pRegion->pFree = (PDETOUR_TRAMPOLINE)pFree;
        goto found_region;
    }

    CB_DEBUG(("Couldn't find available memory region!"));
    return NULL;
}
#endif // end DETOURS_MIPS64

static void detour_free_trampoline(PDETOUR_TRAMPOLINE pTrampoline)
{
    PDETOUR_REGION pRegion = (PDETOUR_REGION)
        ((ULONG_PTR)pTrampoline & ~(ULONG_PTR)0xffff);
//#if defined(DETOURS_X86) || defined(DETOURS_X64) || defined(DETOURS_ARM) || defined(DETOURS_ARM64)
#if defined(DETOURS_X86) || defined(DETOURS_X64) || defined(DETOURS_ARM) || defined(DETOURS_ARM64) || defined(DETOURS_MIPS64)
    if (pTrampoline->IsExecutedPtr != NULL) {
        delete pTrampoline->IsExecutedPtr;
    }
    if (pTrampoline->OutHandle != NULL) {
        delete pTrampoline->OutHandle;
    }
    if (GlobalSlotList[pTrampoline->HLSIndex] == pTrampoline->HLSIdent)
    {
        GlobalSlotList[pTrampoline->HLSIndex] = 0;
    }
#endif
    memset(pTrampoline, 0, sizeof(*pTrampoline));

#if 0
    pTrampoline->pbRemain = (PBYTE)pRegion->pFree;
    pRegion->pFree = pTrampoline;
#endif
}

static BOOL detour_is_region_empty(PDETOUR_REGION pRegion)
{
    // Stop if the region isn't a region (this would be bad).
    if (pRegion->dwSignature != DETOUR_REGION_SIGNATURE) {
        return FALSE;
    }

    PBYTE pbRegionBeg = (PBYTE)pRegion;
    PBYTE pbRegionLim = pbRegionBeg + DETOUR_REGION_SIZE;

    // Stop if any of the trampolines aren't free.
    PDETOUR_TRAMPOLINE pTrampoline = ((PDETOUR_TRAMPOLINE)pRegion) + 1;
    for (int i = 0; i < DETOUR_TRAMPOLINES_PER_REGION; i++) {
        if (pTrampoline[i].pbRemain != NULL &&
            (pTrampoline[i].pbRemain < pbRegionBeg ||
                pTrampoline[i].pbRemain >= pbRegionLim)) {
            return FALSE;
        }
    }

    // OK, the region is empty.
    return TRUE;
}

static void detour_free_unused_trampoline_regions()
{
    PDETOUR_REGION *ppRegionBase = &s_pRegions;
    PDETOUR_REGION pRegion = s_pRegions;

    while (pRegion != NULL) {
        if (detour_is_region_empty(pRegion)) {
            *ppRegionBase = pRegion->pNext;

            munmap(pRegion, DETOUR_REGION_SIZE);
            //VirtualFree(pRegion, 0, MEM_RELEASE);
            s_pRegion = NULL;
        }
        else {
            ppRegionBase = &pRegion->pNext;
        }
        pRegion = *ppRegionBase;
    }
}

///////////////////////////////////////////////////////// Transaction Structs.
//
struct DetourThread
{
    DetourThread *      pNext;
    HANDLE              hThread;
};

struct DetourOperation
{
    DetourOperation *   pNext;
    BOOL                fIsRemove;
    PBYTE *             ppbPointer;
    PBYTE               pbTarget;
    PDETOUR_TRAMPOLINE  pTrampoline;
    ULONG               dwPerm;
};

static BOOL                 s_fIgnoreTooSmall = FALSE;
static BOOL                 s_fRetainRegions = FALSE;

static LONG                 s_nPendingThreadId = 0; // Thread owning pending transaction.
static LONG                 s_nPendingError = NO_ERROR;
static PVOID *              s_ppPendingError = NULL;
static DetourThread *       s_pPendingThreads = NULL;
static DetourOperation *    s_pPendingOperations = NULL;

//////////////////////////////////////////////////////////////////////////////
//
PVOID DetourCodeFromPointer(_In_ PVOID pPointer,
    _Out_opt_ PVOID *ppGlobals)
{
    return detour_skip_jmp((PBYTE)pPointer, ppGlobals);
}

//////////////////////////////////////////////////////////// Transaction APIs.
//
BOOL DetourSetIgnoreTooSmall(_In_ BOOL fIgnore)
{
    BOOL fPrevious = s_fIgnoreTooSmall;
    s_fIgnoreTooSmall = fIgnore;
    return fPrevious;
}

BOOL DetourSetRetainRegions(_In_ BOOL fRetain)
{
    BOOL fPrevious = s_fRetainRegions;
    s_fRetainRegions = fRetain;
    return fPrevious;
}

PVOID DetourSetSystemRegionLowerBound(_In_ PVOID pSystemRegionLowerBound)
{
    PVOID pPrevious = s_pSystemRegionLowerBound;
    s_pSystemRegionLowerBound = pSystemRegionLowerBound;
    return pPrevious;
}

PVOID DetourSetSystemRegionUpperBound(_In_ PVOID pSystemRegionUpperBound)
{
    PVOID pPrevious = s_pSystemRegionUpperBound;
    s_pSystemRegionUpperBound = pSystemRegionUpperBound;
    return pPrevious;
}

LONG DetourTransactionBegin()
{
    // Only one transaction is allowed at a time.
    _Benign_race_begin_
        if (s_nPendingThreadId != 0) {
            return ERROR_INVALID_OPERATION;
        }
    _Benign_race_end_

        // Make sure only one thread can start a transaction.
        if (InterlockedCompareExchange(&s_nPendingThreadId, (LONG)pthread_self(), 0) != 0) {
            return ERROR_INVALID_OPERATION;
        }

    s_pPendingOperations = NULL;
    s_pPendingThreads = NULL;
    s_ppPendingError = NULL;

    // Make sure the trampoline pages are writable.
    s_nPendingError = detour_writable_trampoline_regions();

    return s_nPendingError;
}

LONG DetourTransactionAbort()
{
    if (s_nPendingThreadId != (LONG)pthread_self()) {
        return ERROR_INVALID_OPERATION;
    }

    // Restore all of the page permissions.
    for (DetourOperation *o = s_pPendingOperations; o != NULL;) {
        // We don't care if this fails, because the code is still accessible.
        //DWORD dwOld;
        //VirtualProtect(o->pbTarget, o->pTrampoline->cbRestore,
        //  o->dwPerm, &dwOld);
        mprotect(detour_get_page(o->pbTarget), detour_get_page_size(), PAGE_EXECUTE_READ);
        if (!o->fIsRemove) {
            if (o->pTrampoline) {
                detour_free_trampoline(o->pTrampoline);
                o->pTrampoline = NULL;
            }
        }

        DetourOperation *n = o->pNext;
        delete o;
        o = n;
    }
    s_pPendingOperations = NULL;

    // Make sure the trampoline pages are no longer writable.
    detour_runnable_trampoline_regions();

    // Resume any suspended threads.
    for (DetourThread *t = s_pPendingThreads; t != NULL;) {
        // There is nothing we can do if this fails.
        //ResumeThread(t->hThread);

        DetourThread *n = t->pNext;
        delete t;
        t = n;
    }
    s_pPendingThreads = NULL;
    s_nPendingThreadId = 0;

    return NO_ERROR;
}

LONG DetourTransactionCommit()
{
    return DetourTransactionCommitEx(NULL);
}

static BYTE detour_align_from_trampoline(PDETOUR_TRAMPOLINE pTrampoline, BYTE obTrampoline)
{
    for (LONG n = 0; n < ARRAYSIZE(pTrampoline->rAlign); n++) {
        if (pTrampoline->rAlign[n].obTrampoline == obTrampoline) {
            return pTrampoline->rAlign[n].obTarget;
        }
    }
    return 0;
}

static LONG detour_align_from_target(PDETOUR_TRAMPOLINE pTrampoline, LONG obTarget)
{
    for (LONG n = 0; n < ARRAYSIZE(pTrampoline->rAlign); n++) {
        if (pTrampoline->rAlign[n].obTarget == obTarget) {
            return pTrampoline->rAlign[n].obTrampoline;
        }
    }
    return 0;
}
static ULONG ___TrampolineSize = 0;


#ifdef DETOURS_X64
extern "C" {
    //extern void Trampoline_ASM_x64();
    extern void* trampoline_data_x64;
    extern void(*trampoline_template_x64)();
}
#endif

#ifdef DETOURS_X86
extern "C" void Trampoline_ASM_x86();
#endif

#ifdef DETOURS_MIPS64
extern "C" {
    extern void* trampoline_data_mips64;
    extern void(*trampoline_template_mips64)();
}
#endif

#ifdef DETOURS_ARM
//extern "C" void Trampoline_ASM_ARM();
//extern "C" void Trampoline_ASM_ARM_T();
#endif

#ifdef DETOURS_ARM64
//extern "C" void Trampoline_ASM_ARM64();
#endif

#if defined(DETOURS_X64) || defined(DETOURS_X86)
void* trampoline_template() {


    uintptr_t ret = 0;
#if defined(DETOURS_X64)
    ret = reinterpret_cast<uintptr_t>(&trampoline_template_x64);
#endif
    asm("" : "=rm"(ret)); // force compiler to abandon its assumption that ret is aligned
    //ret &= ~1;
    return reinterpret_cast<void*>(ret);
}

void* trampoline_data() {
#if defined(DETOURS_X64)
    return (&trampoline_data_x64);
#endif
    return nullptr;
}
UCHAR* DetourGetTrampolinePtr()
{
#ifdef DETOURS_X64
    UCHAR* Ptr = (UCHAR*)trampoline_template();
#endif

#ifdef DETOURS_X86
    UCHAR* Ptr = (UCHAR*)Trampoline_ASM_x86;
#endif

    if (*Ptr == 0xE9)
        Ptr += *((int*)(Ptr + 1)) + 5;

    return Ptr;  
}
ULONG GetTrampolineSize()
{
    if (___TrampolineSize != 0)
        return ___TrampolineSize;
    uint32_t code_size_ = reinterpret_cast<uintptr_t>(trampoline_data()) -
        reinterpret_cast<uintptr_t>(trampoline_template());

    ___TrampolineSize = code_size_;
    return ___TrampolineSize;
}
#endif

#ifdef DETOURS_MIPS64
void* trampoline_template()
{
    uintptr_t ret = 0;
    //ret = reinterpret_cast<uintptr_t>(&trampoline_template_mips64);
    asm("" : "=rm"(ret)); // force compiler to abandon its assumption that ret is aligned
    //ret &= ~1;
    return reinterpret_cast<void*>(ret);
}

void* trampoline_data()
{
#ifdef DETOURS_MIPS64
    //return (&trampoline_data_mips64);
    return nullptr;
#endif
    return nullptr;
}

UCHAR* DetourGetTrampolinePtr()
{
    UCHAR* Ptr = (UCHAR*)trampoline_template();
    return Ptr;
}

ULONG GetTrampolineSize()
{
    if (___TrampolineSize != 0)
        return ___TrampolineSize;
    uint32_t code_size_ = reinterpret_cast<uintptr_t>(trampoline_data()) -
        reinterpret_cast<uintptr_t>(trampoline_template());

    ___TrampolineSize = code_size_;
    return ___TrampolineSize;
}
#endif // DETOURS_MIPS64


#if defined(DETOURS_ARM) || defined(DETOURS_ARM64)

extern "C" {

#if defined(_ARM64_)
    extern void* trampoline_data_arm_64;
    extern void(*trampoline_template_arm64)();
#elif defined(_ARM32_) || defined(_ARM_)
    extern void(*trampoline_template_thumb)();
    extern void(*trampoline_template_arm)();
    extern void* trampoline_data_thumb;
    extern void* trampoline_data_arm;
#endif
}
static ULONG ___TrampolineThumbSize = 0;

void* trampoline_template(ULONG isThumb) {
    uintptr_t ret = 0;
#if defined(_ARM64_)
    ret = reinterpret_cast<uintptr_t>(&trampoline_template_arm64);
#elif defined(_ARM32_) || defined(_ARM_)
    if (isThumb) {
        ret = reinterpret_cast<uintptr_t>(&trampoline_template_thumb);
    }
    else {
        ret = reinterpret_cast<uintptr_t>(&trampoline_template_arm);
    }
#endif
    asm("" : "=rm"(ret)); // force compiler to abandon its assumption that ret is aligned
    ret &= ~1;
    return reinterpret_cast<void*>(ret);
}
void* trampoline_data(ULONG isThumb) {
#if defined(_ARM64_)
    return (&trampoline_data_arm_64);
#elif defined(_ARM32_) || defined(_ARM_)
    if (isThumb) {
        return &trampoline_data_thumb;
    }
    else {
        return &trampoline_data_arm;
    }
#endif
    return nullptr;
}

UCHAR* DetourGetArmTrampolinePtr(ULONG isThumb)
{
    // bypass possible Visual Studio debug jump table
    UCHAR* Ptr = NULL;
#if defined(DETOURS_ARM)
    if (isThumb) {
        Ptr = static_cast<UCHAR*>(trampoline_template(isThumb));
    }
    else {
        Ptr = static_cast<UCHAR*>(trampoline_template(isThumb));
    }
#elif defined(DETOURS_ARM64)
    Ptr = (UCHAR*)trampoline_template(NULL);
#endif
    return Ptr;
}
ULONG GetTrampolineSize(ULONG isThumb)
{
    if (isThumb ) {
        if (___TrampolineThumbSize != 0) {
            return ___TrampolineThumbSize;
        }
    }
    else {
        if (___TrampolineSize != 0) {
            return ___TrampolineSize;
        }
    }
    uint32_t code_size_ = reinterpret_cast<uintptr_t>(trampoline_data(isThumb)) -
        reinterpret_cast<uintptr_t>(trampoline_template(isThumb));
    if (isThumb) {
        ___TrampolineThumbSize = code_size_;
    }
    else {
        ___TrampolineSize = code_size_;
    }
    return code_size_;
}
#endif

ULONGLONG BarrierIntro(DETOUR_TRAMPOLINE* InHandle, void* InRetAddr, void** InAddrOfRetAddr)
{
    /*
    Description:

    Will be called from assembler code and enters the
    thread deadlock barrier.
    */
    LPTHREAD_RUNTIME_INFO        Info;
    RUNTIME_INFO*                Runtime;
    BOOL                        Exists;

    CB_DEBUG((boost::format("Barrier Intro InHandle=%p, InRetAddr=%p, InAddrOfRetAddr=%p")
        % InHandle % InRetAddr % InAddrOfRetAddr
        ).str());

#if defined(DETOURS_X64) || defined(DETOURS_ARM) || defined(DETOURS_ARM64)
    InHandle = (DETOUR_TRAMPOLINE*)((PBYTE)(InHandle)-(sizeof(DETOUR_TRAMPOLINE) - DETOUR_TRAMPOLINE_CODE_SIZE));
#endif

#if defined(DETOURS_MIPS64)
    InHandle = (DETOUR_TRAMPOLINE*)((PBYTE)(InHandle)-(sizeof(DETOUR_TRAMPOLINE) - DETOUR_TRAMPOLINE_CODE_SIZE));
#endif

    // are we in OS loader lock?
    if (IsLoaderLock())
    {
        /*
        Execution of managed code or even any other code within any loader lock
        may lead into unpredictable application behavior and therefore we just
        execute without intercepting the call...
        */

        /*  !!Note that the assembler code does not invoke DetourBarrierOutro() in this case!! */

        return FALSE;
    }

    // open pointer table
    Exists = TlsGetCurrentValue(&Unit.TLS, &Info);

    if (!Exists)
    {
        if (!TlsAddCurrentThread(&Unit.TLS))
            return FALSE;
    }

    /*
    To minimize APIs that can't be hooked, we are now entering the self protection.
    This will allow anybody to hook any APIs except those required to setup
    self protection.

    Self protection prevents any further hook interception for the current fiber,
    while setting up the "Thread Deadlock Barrier"...
    */
    if (!AcquireSelfProtection())
    {
        /*  !!Note that the assembler code does not invoke DetourBarrierOutro() in this case!! */

        return FALSE;
    }

    DETOUR_ASSERT(InHandle->HLSIndex < MAX_HOOK_COUNT, "detours.cpp - InHandle->HLSIndex < MAX_HOOK_COUNT");

    if (!Exists)
    {
        TlsGetCurrentValue(&Unit.TLS, &Info);

        Info->Entries = (RUNTIME_INFO*)RtlAllocateMemory(TRUE, sizeof(RUNTIME_INFO) * MAX_HOOK_COUNT);

        if (Info->Entries == NULL)
            goto DONT_INTERCEPT;
    }

    // get hook runtime info...
    Runtime = &Info->Entries[InHandle->HLSIndex];

    if (Runtime->HLSIdent != InHandle->HLSIdent)
    {
        // just reset execution information
        Runtime->HLSIdent = InHandle->HLSIdent;
        Runtime->IsExecuting = FALSE;
    }

    // detect loops in hook execution hiearchy.
    if (Runtime->IsExecuting)
    {
        /*
        This implies that actually the handler has invoked itself. Because of
        the special HookLocalStorage, this is now also signaled if other
        hooks invoked by the related handler are calling it again.

        I call this the "Thread deadlock barrier".

        !!Note that the assembler code does not invoke DetourBarrierOutro() in this case!!
        */

        goto DONT_INTERCEPT;
    }

    Info->Callback = InHandle->Callback;
    Info->Current = Runtime;

    /*
    Now we will negotiate thread/process access based on global and local ACL...
    */
    Runtime->IsExecuting = IsThreadIntercepted(&InHandle->LocalACL, pthread_self());

    if (!Runtime->IsExecuting)
        goto DONT_INTERCEPT;

    // save some context specific information
    Runtime->RetAddress = InRetAddr;
    Runtime->AddrOfRetAddr = InAddrOfRetAddr;

    ReleaseSelfProtection();
    
    return TRUE;

DONT_INTERCEPT:
    /*  !!Note that the assembler code does not invoke UnmanagedHookOutro() in this case!! */

    if (Info != NULL)
    {
        Info->Current = NULL;
        Info->Callback = NULL;

        ReleaseSelfProtection();
    }
    
    return FALSE;
}
void* BarrierOutro(DETOUR_TRAMPOLINE* InHandle, void** InAddrOfRetAddr)
{
    CB_DEBUG((boost::format("Barrier Outro InHandle=%p, InAddrOfRetAddr=%p")
        % InHandle % InAddrOfRetAddr
        ).str());
    
    /*
    Description:
    
    Will just reset the "thread deadlock barrier" for the current hook handler and provides
    some important integrity checks.

    The hook handle is just passed through, because the assembler code has no chance to
    save it in any efficient manner at this point of execution...
    */
    RUNTIME_INFO*            Runtime;
    LPTHREAD_RUNTIME_INFO    Info;
    
#if defined(DETOURS_X64) || defined(DETOURS_ARM) || defined(DETOURS_ARM64)
    InHandle = (DETOUR_TRAMPOLINE*)((PBYTE)(InHandle)-(sizeof(DETOUR_TRAMPOLINE) - DETOUR_TRAMPOLINE_CODE_SIZE));
    //InHandle -= 1;
#endif

#if defined(DETOURS_MIPS64)
    InHandle = (DETOUR_TRAMPOLINE*)((PBYTE)(InHandle)-(sizeof(DETOUR_TRAMPOLINE) - DETOUR_TRAMPOLINE_CODE_SIZE));
    //InHandle -= 1;
#endif
    
    DETOUR_ASSERT(AcquireSelfProtection(), "detours.cpp - AcquireSelfProtection()");

    DETOUR_ASSERT(TlsGetCurrentValue(&Unit.TLS, &Info) && (Info != NULL), "detours.cpp - TlsGetCurrentValue(&Unit.TLS, &Info) && (Info != NULL)");

    Runtime = &Info->Entries[InHandle->HLSIndex];

    // leave handler context
    Info->Current = NULL;
    Info->Callback = NULL;

    DETOUR_ASSERT(Runtime != NULL, "detours.cpp - Runtime != NULL");

    DETOUR_ASSERT(Runtime->IsExecuting, "detours.cpp - Runtime->IsExecuting");

    Runtime->IsExecuting = FALSE;

#ifndef DETOURS_MIPS64
    DETOUR_ASSERT(*InAddrOfRetAddr == NULL, "detours.cpp - *InAddrOfRetAddr == NULL");
#endif

    *InAddrOfRetAddr = Runtime->RetAddress;

    ReleaseSelfProtection();

    return InHandle;
}

TRACED_HOOK_HANDLE DetourGetHookHandleForFunction(PDETOUR_TRAMPOLINE pTrampoline)
{
    if (pTrampoline != NULL) {
        return pTrampoline->OutHandle;
    }
    return NULL;
}
LONG DetourSetCallbackForLocalHook(PDETOUR_TRAMPOLINE pTrampoline, PVOID pCallback)
{
    if (pTrampoline != NULL) {
        pTrampoline->Callback = pCallback;
        return 0;
    }

    return -1;
}

VOID InsertTraceHandle(PDETOUR_TRAMPOLINE pTrampoline)
{
    if (pTrampoline != NULL && pTrampoline->OutHandle != NULL) {
        memset(&pTrampoline->LocalACL, 0, sizeof(HOOK_ACL));

        TRACED_HOOK_HANDLE OutHandle = new HOOK_TRACE_INFO();

        pTrampoline->OutHandle = OutHandle;

        OutHandle->Link = pTrampoline;
    }
}
LONG AddTrampolineToGlobalList(PDETOUR_TRAMPOLINE pTrampoline)
{
    ULONG   Index;
    BOOL    Exists;
    // register in global HLS list
    RtlAcquireLock(&GlobalHookLock);
    {
        pTrampoline->HLSIdent = UniqueIDCounter++;

        Exists = FALSE;

        for (Index = 0; Index < MAX_HOOK_COUNT; Index++)
        {
            if (GlobalSlotList[Index] == 0)
            {
                GlobalSlotList[Index] = pTrampoline->HLSIdent;

                pTrampoline->HLSIndex = Index;

                Exists = TRUE;

                break;
            }
        }
    }
    RtlReleaseLock(&GlobalHookLock);

    return Exists;
}

LONG DetourExport DetourUninstallHook(TRACED_HOOK_HANDLE InHandle)
{
    /*
    Description:

    Removes the given hook. To also release associated resources,
    you will have to call DetourWaitForPendingRemovals(). In any case
    your hook handler will never be executed again, after calling this
    method.

    Parameters:

    - InHandle

    A traced hook handle. If the hook is already removed, this method
    will still return STATUS_SUCCESS.
    */
    LONG error = -1;

    PDETOUR_TRAMPOLINE      Hook = NULL;
    LONG                    NtStatus = -1;
    BOOLEAN                 IsAllocated = FALSE;

    if (!IsValidPointer(InHandle, sizeof(HOOK_TRACE_INFO)))
        return FALSE;

    RtlAcquireLock(&GlobalHookLock);
    {
        if ((InHandle->Link != NULL) && DetourIsValidHandle(InHandle, &Hook))
        {
            DetourTransactionBegin();
            DetourUpdateThread(pthread_self());
            DetourDetach(&(PVOID&)Hook->OldProc, Hook->pbDetour);

            InHandle->Link = NULL;

            if (Hook->HookProc != NULL)
            {
                Hook->HookProc = NULL;

                IsAllocated = TRUE;
            }

            error = DetourTransactionCommit();

            if (!IsAllocated)
            {
                RtlReleaseLock(&GlobalHookLock);

                RETURN;
            }
        }
    }
    RtlReleaseLock(&GlobalHookLock);

    RETURN(STATUS_SUCCESS);

FINALLY_OUTRO:
    return NtStatus;
}

LONG DetourIsThreadIntercepted(
    TRACED_HOOK_HANDLE InHook,
    ULONG InThreadID,
    BOOL* OutResult)
{
    /*
    Description:

    This method will negotiate whether a given thread passes
    the ACLs and would invoke the related hook handler. Refer
    to the source code of Is[Thread/Process]Intercepted() for more information
    about the implementation.

    */
    LONG                NtStatus;
    PLOCAL_HOOK_INFO    Handle;

    if (!DetourIsValidHandle(InHook, &Handle))
        THROW(-1, (PWCHAR)"The given hook handle is invalid or already disposed.");

    if (!IsValidPointer(OutResult, sizeof(BOOL)))
        THROW(-3, (PWCHAR)"Invalid pointer for result storage.");

    *OutResult = IsThreadIntercepted(&Handle->LocalACL, InThreadID);

    RETURN;

THROW_OUTRO:
FINALLY_OUTRO:
    return NtStatus;
}

LONG DetourSetInclusiveACL(
    ULONG* InThreadIdList,
    ULONG InThreadCount,
    TRACED_HOOK_HANDLE InHandle)
{
    /*
    Description:

    Sets an inclusive hook local ACL based on the given thread ID list.
    Only threads in this list will be intercepted by the hook. If the
    global ACL also is inclusive, then all threads stated there are
    intercepted too.

    Parameters:
    - InThreadIdList
    An array of thread IDs. If you specific zero for an entry in this array,
    it will be automatically replaced with the calling thread ID.

    - InThreadCount
    The count of entries listed in the thread ID list. This value must not exceed
    MAX_ACE_COUNT!

    - InHandle
    The hook handle whose local ACL is going to be set.
    */
    PLOCAL_HOOK_INFO        Handle;

    if (!DetourIsValidHandle(InHandle, &Handle))
        return -3;

    return DetourSetACL(&Handle->LocalACL, FALSE, InThreadIdList, InThreadCount);
}

LONG DetourGetHookBypassAddress(
    TRACED_HOOK_HANDLE InHook,
    PVOID** OutAddress)
{
    /*
    Description:

    Retrieves the address to bypass the hook. Using the returned value to call the original
    function bypasses all thread safety measures and must be used with care.
    This function should be called each time the address is required to ensure the hook  and
    associated memory is still valid at the time of use.
    CAUTION:
    This must be used with extreme caution. If the hook is uninstalled and pending hooks
    removed, the address returned by this function will no longer point to valid memory and
    attempting to use the address will result in unexpected behaviour, most likely crashing
    the process.

    Parameters:

    - InHook

    The hook to retrieve the relocated entry point for.

    - OutAddress

    Upon successfully retrieving the hook details this will contain
    the address of the relocated function entry point. This address
    can be used to call the original function from outside of a hook
    while still bypassing the hook.

    Returns:

    STATUS_SUCCESS             - OutAddress will contain the result
    STATUS_INVALID_PARAMETER_1 - the hook is invalid
    STATUS_INVALID_PARAMETER_3 - the target pointer is invalid

    */
    LONG                NtStatus;
    PLOCAL_HOOK_INFO    Handle;

    if (!DetourIsValidHandle(InHook, &Handle))
        THROW(-1, (PWCHAR)"The given hook handle is invalid or already disposed.");

    if (!IsValidPointer(OutAddress, sizeof(PVOID*)))
        THROW(-3, (PWCHAR)"Invalid pointer for result storage.");

    *OutAddress = (PVOID*)Handle->OldProc;

    RETURN;

THROW_OUTRO:
FINALLY_OUTRO:
    return NtStatus;
}

LONG DetourExport DetourInstallHook(
    void* InEntryPoint,
    void* InHookProc,
    void* InCallback,
    TRACED_HOOK_HANDLE OutHandle)
{
    /*
    Description:

    Installs a hook at the given entry point, redirecting all
    calls to the given hooking method. The returned handle will
    either be released on library unloading or explicitly through
    DetourUninstallHook() or DetourUninstallAllHooks().

    Parameters:

    - InEntryPoint

    An entry point to hook. Not all entry points are hookable. In such
    a case STATUS_NOT_SUPPORTED will be returned.

    - InHookProc

    The method that should be called instead of the given entry point.
    Please note that calling convention, parameter count and return value
    shall match EXACTLY!

    - InCallback

    An uninterpreted callback later available through
    DetourBarrierGetCallback().

    - OutPHandle

    The memory portion supplied by *OutHandle is expected to be preallocated
    by the caller. This structure is then filled by the method on success and
    must stay valid for hook-life time. Only if you explicitly call one of
    the hook uninstallation APIs, you can safely release the handle memory.

    Returns:

    STATUS_NO_MEMORY

    Unable to allocate memory around the target entry point.

    STATUS_NOT_SUPPORTED

    The target entry point contains unsupported instructions.

    STATUS_INSUFFICIENT_RESOURCES

    The limit of MAX_HOOK_COUNT simultaneous hooks was reached.

    */

    LONG    NtStatus = -1;
    LONG    error = -1;
    PDETOUR_TRAMPOLINE pTrampoline = NULL;

    // validate parameters
    if (!IsValidPointer(InEntryPoint, 1))
        THROW(-2, (PWCHAR)"Invalid entry point.");

    if (!IsValidPointer(InHookProc, 1))
        THROW(-3, (PWCHAR)"Invalid hook procedure.");
    
    if (!IsValidPointer(OutHandle, sizeof(HOOK_TRACE_INFO)))
        THROW(-4, (PWCHAR)"The hook handle storage is expected to be allocated by the caller.");

    if (OutHandle->Link != NULL)
        THROW(-5, (PWCHAR)"The given trace handle seems to already be associated with a hook.");

    error = DetourTransactionBegin();

    error = DetourUpdateThread(pthread_self());

    error = DetourAttachEx(&(PVOID &)InEntryPoint, InHookProc, &pTrampoline, NULL, NULL);

    if (error == NO_ERROR)
    {
        DetourSetCallbackForLocalHook(pTrampoline, InCallback);
    }
    error = DetourTransactionCommit();
    if (OutHandle != NULL && error == NO_ERROR)
    {
        TRACED_HOOK_HANDLE handle = DetourGetHookHandleForFunction(pTrampoline);
        if (handle != NULL) {
            OutHandle->Link = handle->Link;
        }        
    }
THROW_OUTRO:

    return error;
}

LONG DetourSetExclusiveACL(
    ULONG* InThreadIdList,
    ULONG InThreadCount,
    TRACED_HOOK_HANDLE InHandle)
{
    /*
    Description:

    Sets an exclusive hook local ACL based on the given thread ID list.

    Parameters:
    - InThreadIdList
    An array of thread IDs. If you specific zero for an entry in this array,
    it will be automatically replaced with the calling thread ID.

    - InThreadCount
    The count of entries listed in the thread ID list. This value must not exceed
    MAX_ACE_COUNT!

    - InHandle
    The hook handle whose local ACL is going to be set.
    */
    PLOCAL_HOOK_INFO        Handle;

    if (!DetourIsValidHandle(InHandle, &Handle))
        return -3;

    return DetourSetACL(&Handle->LocalACL, TRUE, InThreadIdList, InThreadCount);
}

LONG DetourTransactionCommitEx(_Out_opt_ PVOID **pppFailedPointer)
{
    if (pppFailedPointer != NULL) {
        // Used to get the last error.
        *pppFailedPointer = s_ppPendingError;
    }
    if (s_nPendingThreadId != (LONG)pthread_self()) {
        return ERROR_INVALID_OPERATION;
    }

    // If any of the pending operations failed, then we abort the whole transaction.
    if (s_nPendingError != NO_ERROR) {
        DetourTransactionAbort();
        return s_nPendingError;
    }

    // Common variables.
    DetourOperation *o;
    DetourThread *t;
    BOOL freed = FALSE;

    // Insert or remove each of the detours.
    for (o = s_pPendingOperations; o != NULL; o = o->pNext) {
        if (o->fIsRemove) {
            CopyMemory(o->pbTarget,
                o->pTrampoline->rbRestore,
                o->pTrampoline->cbRestore);
#ifdef DETOURS_IA64
            *o->ppbPointer = (PBYTE)o->pTrampoline->ppldTarget;
#endif // DETOURS_IA64

#ifdef DETOURS_X86
            *o->ppbPointer = o->pbTarget;
#endif // DETOURS_X86

#ifdef DETOURS_X64
            *o->ppbPointer = o->pbTarget;
#endif // DETOURS_X64

#ifdef DETOURS_MIPS64
            *o->ppbPointer = o->pbTarget;
#endif // DETOURS_MIPS64

#ifdef DETOURS_ARM
            *o->ppbPointer = DETOURS_PBYTE_TO_PFUNC(o->pbTarget);
#endif // DETOURS_ARM

#ifdef DETOURS_ARM64
            *o->ppbPointer = o->pbTarget;
#endif // DETOURS_ARM
        }
        else {
            CB_DEBUG((boost::format("detours: pbTramp =%p, pbRemain=%p, pbDetour=%p, cbRestore=%d")
                % (void*)o->pTrampoline
                % (void*)o->pTrampoline->pbRemain
                % (void*)o->pTrampoline->pbDetour
                % (int)o->pTrampoline->cbRestore
                ).str());

            CB_DEBUG((boost::format("detours: pbTarget=%p: "
                "%02x %02x %02x %02x "
                "%02x %02x %02x %02x "
                "%02x %02x %02x %02x [before]")
                % (void*)o->pbTarget
                % (int)o->pbTarget[0] % (int)o->pbTarget[1] % (int)o->pbTarget[2] % (int)o->pbTarget[3]
                % (int)o->pbTarget[4] % (int)o->pbTarget[5] % (int)o->pbTarget[6] % (int)o->pbTarget[7]
                % (int)o->pbTarget[8] % (int)o->pbTarget[9] % (int)o->pbTarget[10] % (int)o->pbTarget[11]
                ).str());

#ifdef DETOURS_IA64
            ((DETOUR_IA64_BUNDLE*)o->pbTarget)
                ->SetBrl((UINT64)&o->pTrampoline->bAllocFrame);
            *o->ppbPointer = (PBYTE)&o->pTrampoline->pldTrampoline;
#endif // DETOURS_IA64

#ifdef DETOURS_X64
            PBYTE trampoline = DetourGetTrampolinePtr();
            const ULONG TrampolineSize = GetTrampolineSize();
            if (TrampolineSize > DETOUR_TRAMPOLINE_CODE_SIZE) {
                //error, handle this better
                CB_DEBUG((boost::format("detours: TrampolineSize > DETOUR_TRAMPOLINE_CODE_SIZE (%08X != %08X)")
                    % TrampolineSize % DETOUR_TRAMPOLINE_CODE_SIZE
                    ).str());
                CB_DEBUG("Invalid trampoline size: " << TrampolineSize);
            }
            PBYTE endOfTramp = (PBYTE)&o->pTrampoline->rbTrampolineCode;
            memcpy(endOfTramp, trampoline, TrampolineSize);
            o->pTrampoline->HookIntro = (PVOID)BarrierIntro;
            o->pTrampoline->HookOutro = (PVOID)BarrierOutro;
            o->pTrampoline->Trampoline = endOfTramp;
            o->pTrampoline->OldProc = o->pTrampoline->rbCode;
            o->pTrampoline->HookProc = o->pTrampoline->pbDetour;
            o->pTrampoline->IsExecutedPtr = new int();

            detour_gen_jmp_indirect(o->pTrampoline->rbCodeIn, (PBYTE*)&o->pTrampoline->Trampoline);
            PBYTE pbCode = detour_gen_jmp_immediate(o->pbTarget, o->pTrampoline->rbCodeIn);
            pbCode = detour_gen_brk(pbCode, o->pTrampoline->pbRemain);
            *o->ppbPointer = o->pTrampoline->rbCode;
            UNREFERENCED_PARAMETER(pbCode);
#endif // DETOURS_X64

#ifdef DETOURS_X86
            PBYTE trampoline = DetourGetTrampolinePtr();
            const ULONG TrampolineSize = GetTrampolineSize();
            if (TrampolineSize > DETOUR_TRAMPOLINE_CODE_SIZE) {
                //error, handle this better
                CB_DEBUG((boost::format("detours: TrampolineSize > DETOUR_TRAMPOLINE_CODE_SIZE (%08X != %08X)")
                    % TrampolineSize % DETOUR_TRAMPOLINE_CODE_SIZE
                    ).str());
                CB_DEBUG("Invalid trampoline size: " << TrampolineSize);
            }
            PBYTE endOfTramp = (PBYTE)&o->pTrampoline->rbTrampolineCode;
            memcpy(endOfTramp, trampoline, TrampolineSize);
            o->pTrampoline->HookIntro = BarrierIntro;
            o->pTrampoline->HookOutro = BarrierOutro;
            o->pTrampoline->Trampoline = endOfTramp;
            o->pTrampoline->OldProc = o->pTrampoline->rbCode;
            o->pTrampoline->HookProc = o->pTrampoline->pbDetour;
            o->pTrampoline->IsExecutedPtr = new int();
            PBYTE Ptr = (PBYTE)o->pTrampoline->Trampoline;
            for (ULONG Index = 0; Index < TrampolineSize; Index++)
            {
                switch (*((ULONG*)(Ptr)))
                {
                /*Handle*/            case 0x1A2B3C05: *((ULONG*)Ptr) = (ULONG)o->pTrampoline; break;
                /*UnmanagedIntro*/    case 0x1A2B3C03: *((ULONG*)Ptr) = (ULONG)o->pTrampoline->HookIntro; break;
                /*OldProc*/           case 0x1A2B3C01: *((ULONG*)Ptr) = (ULONG)o->pTrampoline->OldProc; break;
                /*Ptr:NewProc*/       case 0x1A2B3C07: *((ULONG*)Ptr) = (ULONG)&o->pTrampoline->HookProc; break;
                /*NewProc*/           case 0x1A2B3C00: *((ULONG*)Ptr) = (ULONG)o->pTrampoline->HookProc; break;
                /*UnmanagedOutro*/    case 0x1A2B3C06: *((ULONG*)Ptr) = (ULONG)o->pTrampoline->HookOutro; break;
                /*IsExecuted*/        case 0x1A2B3C02: *((ULONG*)Ptr) = (ULONG)o->pTrampoline->IsExecutedPtr; break;
                /*RetAddr*/           case 0x1A2B3C04: *((ULONG*)Ptr) = (ULONG)((PBYTE)o->pTrampoline->Trampoline + 92); break;
                }

                Ptr++;
            }

            PBYTE pbCode = detour_gen_jmp_immediate(o->pbTarget, (PBYTE)o->pTrampoline->Trampoline);
            pbCode = detour_gen_brk(pbCode, o->pTrampoline->pbRemain);
            *o->ppbPointer = o->pTrampoline->rbCode;
            UNREFERENCED_PARAMETER(pbCode);
#endif // DETOURS_X86

#ifdef DETOURS_MIPS64
#if 0
            PBYTE trampoline = DetourGetTrampolinePtr();
            const ULONG TrampolineSize = GetTrampolineSize();
            if (TrampolineSize > DETOUR_TRAMPOLINE_CODE_SIZE) {
                //error, handle this better
                CB_DEBUG("Invalid trampoline size: " << TrampolineSize);
            }
#endif

            PBYTE endOfTramp = (PBYTE)&o->pTrampoline->rbTrampolineCode;
#if 1
            // hard code trampoline assembly code
            std::string asm_hard_code = generate_asm_hard_code();
            memcpy(o->pTrampoline->rbTrampolineCode, asm_hard_code.c_str(), asm_hard_code.size());
            memcpy(endOfTramp, asm_hard_code.c_str(), asm_hard_code.size());
#else
            memcpy(endOfTramp, trampoline, TrampolineSize);
#endif

            o->pTrampoline->HookIntro = (PVOID)BarrierIntro;
            o->pTrampoline->HookOutro = (PVOID)BarrierOutro;
            o->pTrampoline->Trampoline = endOfTramp;
            o->pTrampoline->OldProc = o->pTrampoline->rbCode;
            o->pTrampoline->HookProc = o->pTrampoline->pbDetour;
            o->pTrampoline->IsExecutedPtr = new int();

#if 0
            detour_gen_jmp_indirect(o->pTrampoline->rbCodeIn, (PBYTE*)&o->pTrampoline->Trampoline);
#else
            detour_gen_jmp_immediate(o->pTrampoline->rbCodeIn, (PBYTE)(o->pTrampoline->Trampoline));
#endif
            PBYTE pbCode = detour_gen_jmp_immediate(o->pbTarget, o->pTrampoline->rbCodeIn);
            pbCode = detour_gen_brk(pbCode, o->pTrampoline->pbRemain);
            *o->ppbPointer = o->pTrampoline->rbCode;
            UNREFERENCED_PARAMETER(pbCode);
#endif // DETOURS_MIPS64

#ifdef DETOURS_ARM
            UCHAR * trampoline = DetourGetArmTrampolinePtr(o->pTrampoline->IsThumbTarget);
            const ULONG TrampolineSize = GetTrampolineSize(o->pTrampoline->IsThumbTarget);
            if (TrampolineSize > DETOUR_TRAMPOLINE_CODE_SIZE) {
                //error, handle this better
                CB_DEBUG((boost::format("detours: TrampolineSize > DETOUR_TRAMPOLINE_CODE_SIZE (%08X != %08X)")
                    % TrampolineSize % DETOUR_TRAMPOLINE_CODE_SIZE
                    ).str());
                CB_DEBUG("Invalid trampoline size: " << TrampolineSize);
            }

            PBYTE endOfTramp = (PBYTE)&o->pTrampoline->rbTrampolineCode;

            PBYTE trampolineStart = align4(trampoline);
            // means thumb_to_arm thunk is not compiled
            uint32_t arm_to_thunk_code_size_offset = 0;
#if not defined(DETOURS_ARM32)
            // otherwise, copy to (trampoline + 4) to offset thumb_to_arm thunk code (byte size = 4)
            if (!o->pTrampoline->IsThumbTarget) {
                arm_to_thunk_code_size_offset = 4;
            }
#endif
            memcpy(endOfTramp + arm_to_thunk_code_size_offset, trampolineStart, TrampolineSize);

            o->pTrampoline->HookIntro = (PVOID)BarrierIntro;
            o->pTrampoline->HookOutro = (PVOID)BarrierOutro;
            if (o->pTrampoline->IsThumbTarget) {
                o->pTrampoline->Trampoline = DETOURS_PBYTE_TO_PFUNC(endOfTramp);
                o->pTrampoline->OldProc = DETOURS_PBYTE_TO_PFUNC(o->pTrampoline->rbCode);
                o->pTrampoline->HookProc = DETOURS_PBYTE_TO_PFUNC(o->pTrampoline->pbDetour);
                *o->ppbPointer = DETOURS_PBYTE_TO_PFUNC(o->pTrampoline->rbCode);
            }
            else {
                o->pTrampoline->Trampoline = (endOfTramp);
                o->pTrampoline->OldProc = (o->pTrampoline->rbCode);
                o->pTrampoline->HookProc = (o->pTrampoline->pbDetour);
                *o->ppbPointer = (o->pTrampoline->rbCode);
            }
            o->pTrampoline->IsExecutedPtr = new int();
            PBYTE pbCode = detour_gen_jmp_immediate(o->pbTarget + o->pTrampoline->IsThumbTarget, NULL,
                (PBYTE)o->pTrampoline->Trampoline + arm_to_thunk_code_size_offset);
            pbCode = detour_gen_brk(pbCode, o->pTrampoline->pbRemain);
    
            UNREFERENCED_PARAMETER(pbCode);
#endif // DETOURS_ARM

#ifdef DETOURS_ARM64
            UCHAR * trampolineStart = DetourGetArmTrampolinePtr(NULL);
            const ULONG TrampolineSize = GetTrampolineSize(NULL);
            if (TrampolineSize > DETOUR_TRAMPOLINE_CODE_SIZE) {
                //error, handle this better
                CB_DEBUG((boost::format("detours: TrampolineSize > DETOUR_TRAMPOLINE_CODE_SIZE (%08X != %08X)")
                    % TrampolineSize % DETOUR_TRAMPOLINE_CODE_SIZE
                    ).str());
                CB_DEBUG("Invalid trampoline size: " << TrampolineSize);
            }
            PBYTE endOfTramp = (PBYTE)&o->pTrampoline->rbTrampolineCode;
            memcpy(endOfTramp, trampolineStart, TrampolineSize);
            
            o->pTrampoline->HookIntro = (PVOID)BarrierIntro;
            o->pTrampoline->HookOutro = (PVOID)BarrierOutro;
            o->pTrampoline->Trampoline = endOfTramp;
            o->pTrampoline->OldProc = o->pTrampoline->rbCode;
            o->pTrampoline->HookProc = o->pTrampoline->pbDetour;
            o->pTrampoline->IsExecutedPtr = new int();
            PBYTE pbCode = detour_gen_jmp_immediate(o->pbTarget, NULL, (PBYTE)o->pTrampoline->Trampoline);
            pbCode = detour_gen_brk(pbCode, o->pTrampoline->pbRemain);
            *o->ppbPointer = o->pTrampoline->rbCode;
            UNREFERENCED_PARAMETER(pbCode);

#endif // DETOURS_ARM64
            
            CB_DEBUG((boost::format("detours: pbTarget=%p: "
                "%02x %02x %02x %02x "
                "%02x %02x %02x %02x "
                "%02x %02x %02x %02x [after]")
                % (void*)o->pbTarget
                % (int)o->pbTarget[0]% (int)o->pbTarget[1]% (int)o->pbTarget[2]% (int)o->pbTarget[3]
                % (int)o->pbTarget[4]% (int)o->pbTarget[5]% (int)o->pbTarget[6]% (int)o->pbTarget[7]
                % (int)o->pbTarget[8]% (int)o->pbTarget[9]% (int)o->pbTarget[10]% (int)o->pbTarget[11]
                ).str());
            
            CB_DEBUG((boost::format("detours: pbTramp =%p: "
                "%02x %02x %02x %02x "
                "%02x %02x %02x %02x "
                "%02x %02x %02x %02x ")
                % (void*)o->pTrampoline
                % (int)o->pTrampoline->rbCode[0] % (int)o->pTrampoline->rbCode[1]
                % (int)o->pTrampoline->rbCode[2] % (int)o->pTrampoline->rbCode[3]
                % (int)o->pTrampoline->rbCode[4] % (int)o->pTrampoline->rbCode[5]
                % (int)o->pTrampoline->rbCode[6] % (int)o->pTrampoline->rbCode[7]
                % (int)o->pTrampoline->rbCode[8] % (int)o->pTrampoline->rbCode[9]
                % (int)o->pTrampoline->rbCode[10] % (int)o->pTrampoline->rbCode[11]
                ).str());

#ifdef DETOURS_IA64
            CB_DEBUG((""));
            CB_DEBUG((boost::format("detours:  &pldTrampoline  =%p")
                % &o->pTrampoline->pldTrampoline
                ).str());
            CB_DEBUG((boost::format("detours:  &bMovlTargetGp  =%p [%p]")
                % &o->pTrampoline->bMovlTargetGp
                % o->pTrampoline->bMovlTargetGp.GetMovlGp()
                ).str());
            CB_DEBUG((boost::format("detours:  &rbCode         =%p [%p]")
                , &o->pTrampoline->rbCode
                , ((DETOUR_IA64_BUNDLE&)o->pTrampoline->rbCode).GetBrlTarget()
                ).str());
            CB_DEBUG((boost::format("detours:  &bBrlRemainEip  =%p [%p]")
                % &o->pTrampoline->bBrlRemainEip
                % o->pTrampoline->bBrlRemainEip.GetBrlTarget()
                ).str());
            CB_DEBUG((boost::format("detours:  &bMovlDetourGp  =%p [%p]")
                % &o->pTrampoline->bMovlDetourGp
                % o->pTrampoline->bMovlDetourGp.GetMovlGp()
                ).str());
            CB_DEBUG((boost::format("detours:  &bBrlDetourEip  =%p [%p]")
                % &o->pTrampoline->bCallDetour
                % o->pTrampoline->bCallDetour.GetBrlTarget()
                ).str());
            CB_DEBUG((boost::format("detours:  pldDetour       =%p [%p]"
                % o->pTrampoline->ppldDetour->EntryPoint
                % o->pTrampoline->ppldDetour->GlobalPointer()
                ).str());
            CB_DEBUG((boost::format("detours:  pldTarget       =%p [%p]"
                % o->pTrampoline->ppldTarget->EntryPoint
                % o->pTrampoline->ppldTarget->GlobalPointer()
                ).str());
            CB_DEBUG((boost::format("detours:  pbRemain        =%p")
                % o->pTrampoline->pbRemain)
                ).str());
            CB_DEBUG((boost::format("detours:  pbDetour        =%p")
                % o->pTrampoline->pbDetour)
                ).str());
            CB_DEBUG((""));
#endif // DETOURS_IA64

            std::string trampoline_string = dumpTrampoline(o->pTrampoline);
            CB_DEBUG((boost::format("dump trampoline:%s")% trampoline_string.c_str()).str());

            AddTrampolineToGlobalList(o->pTrampoline);
        }
    }


    // Update any suspended threads.
    for (t = s_pPendingThreads; t != NULL; t = t->pNext) {
        /*
        CONTEXT cxt;
        cxt.ContextFlags = CONTEXT_CONTROL;

        #undef DETOURS_EIP

        #ifdef DETOURS_X86
        #define DETOURS_EIP         Eip
        #endif // DETOURS_X86

        #ifdef DETOURS_X64
        #define DETOURS_EIP         Rip
        #endif // DETOURS_X64

        #ifdef DETOURS_IA64
        #define DETOURS_EIP         StIIP
        #endif // DETOURS_IA64

        #ifdef DETOURS_ARM
        #define DETOURS_EIP         Pc
        #endif // DETOURS_ARM

        #ifdef DETOURS_ARM64
        #define DETOURS_EIP         Pc
        #endif // DETOURS_ARM64

        typedef ULONG_PTR DETOURS_EIP_TYPE;

        if (GetThreadContext(t->hThread, &cxt)) {
        for (o = s_pPendingOperations; o != NULL; o = o->pNext) {
        if (o->fIsRemove) {
        if (cxt.DETOURS_EIP >= (DETOURS_EIP_TYPE)(ULONG_PTR)o->pTrampoline &&
        cxt.DETOURS_EIP < (DETOURS_EIP_TYPE)((ULONG_PTR)o->pTrampoline
        + sizeof(o->pTrampoline))
        ) {

        cxt.DETOURS_EIP = (DETOURS_EIP_TYPE)
        ((ULONG_PTR)o->pbTarget
        + detour_align_from_trampoline(o->pTrampoline,
        (BYTE)(cxt.DETOURS_EIP
        - (DETOURS_EIP_TYPE)(ULONG_PTR)
        o->pTrampoline)));

        SetThreadContext(t->hThread, &cxt);
        }
        }
        else {
        if (cxt.DETOURS_EIP >= (DETOURS_EIP_TYPE)(ULONG_PTR)o->pbTarget &&
        cxt.DETOURS_EIP < (DETOURS_EIP_TYPE)((ULONG_PTR)o->pbTarget
        + o->pTrampoline->cbRestore)
        ) {

        cxt.DETOURS_EIP = (DETOURS_EIP_TYPE)
        ((ULONG_PTR)o->pTrampoline
        + detour_align_from_target(o->pTrampoline,
        (BYTE)(cxt.DETOURS_EIP
        - (DETOURS_EIP_TYPE)(ULONG_PTR)
        o->pbTarget)));

        SetThreadContext(t->hThread, &cxt);
        }
        }
        }
        }
        */
#undef DETOURS_EIP
    }

    // Restore all of the page permissions and flush the icache.
    //HANDLE hProcess = GetCurrentProcess();
    for (o = s_pPendingOperations; o != NULL;) {

        // We don't care if this fails, because the code is still accessible.
        mprotect(detour_get_page(o->pbTarget), detour_get_page_size(), PAGE_EXECUTE_READ);

        if (o->fIsRemove && o->pTrampoline) {
            detour_free_trampoline(o->pTrampoline);
            o->pTrampoline = NULL;
            freed = true;
        }

        DetourOperation *n = o->pNext;
        delete o;
        o = n;
    }
    s_pPendingOperations = NULL;

    // Free any trampoline regions that are now unused.
    if (freed && !s_fRetainRegions) {
        detour_free_unused_trampoline_regions();
    }

    // Make sure the trampoline pages are no longer writable.
    detour_runnable_trampoline_regions();

    // Resume any suspended threads.
    for (t = s_pPendingThreads; t != NULL;) {
        // There is nothing we can do if this fails.
        //ResumeThread(t->hThread);

        DetourThread *n = t->pNext;
        delete t;
        t = n;
    }
    s_pPendingThreads = NULL;
    s_nPendingThreadId = 0;

    if (pppFailedPointer != NULL) {
        *pppFailedPointer = s_ppPendingError;
    }

    return s_nPendingError;
}

LONG DetourUpdateThread(_In_ pthread_t hThread)
{
    /*
    LONG error;

    // If any of the pending operations failed, then we don't need to do this.
    if (s_nPendingError != NO_ERROR) {
    return s_nPendingError;
    }

    // Silently (and safely) drop any attempt to suspend our own thread.
    if (hThread == GetCurrentThread()) {
    return NO_ERROR;
    }

    DetourThread *t = new NOTHROW DetourThread;
    if (t == NULL) {
    error = ERROR_NOT_ENOUGH_MEMORY;
    fail:
    if (t != NULL) {
    delete t;
    t = NULL;
    }
    s_nPendingError = error;
    s_ppPendingError = NULL;
    return error;
    }

    if (SuspendThread(hThread) == (DWORD)-1) {
    error = GetLastError();
    goto fail;
    }

    t->hThread = hThread;
    t->pNext = s_pPendingThreads;
    s_pPendingThreads = t;
    */
    return NO_ERROR;
}

const unsigned int max_print_buffer_size = 1024;

char buffer_print[max_print_buffer_size];
const char* ___DETOUR_TRACE(const char *format, ...)
{
    memset(buffer_print, 0, max_print_buffer_size);
    va_list arg;

    va_start(arg, format);
    vsnprintf(buffer_print, max_print_buffer_size, format, arg);
    va_end(arg);

    return buffer_print;
}
///////////////////////////////////////////////////////////// Transacted APIs.
//
LONG DetourAttach(_Inout_ PVOID *ppPointer,
    _In_ PVOID pDetour)
{
    return DetourAttachEx(ppPointer, pDetour, NULL, NULL, NULL);
}

LONG DetourAttachEx(_Inout_ PVOID *ppPointer,
    _In_ PVOID pDetour,
    _Out_opt_ PDETOUR_TRAMPOLINE *ppRealTrampoline,
    _Out_opt_ PVOID *ppRealTarget,
    _Out_opt_ PVOID *ppRealDetour)
{
    CB_DEBUG((boost::format("detours: trigger DetourAttachEx(), pbSrc=%p, pDetour=%p")
             % *ppPointer% pDetour
             ).str());
    LONG error = NO_ERROR;

    if (ppRealTrampoline != NULL) {
        *ppRealTrampoline = NULL;
    }
    if (ppRealTarget != NULL) {
        *ppRealTarget = NULL;
    }
    if (ppRealDetour != NULL) {
        *ppRealDetour = NULL;
    }
    if (pDetour == NULL) {
        CB_DEBUG(("empty detour"));
        return ERROR_INVALID_PARAMETER;
    }

    if (s_nPendingThreadId != (LONG)pthread_self()) {
        CB_DEBUG((boost::format("transaction conflict with thread id=%d")% s_nPendingThreadId).str());
        return ERROR_INVALID_OPERATION;
    }

    // If any of the pending operations failed, then we don't need to do this.
    if (s_nPendingError != NO_ERROR) {
        CB_DEBUG((boost::format("pending transaction error=%d")% s_nPendingError).str());
        return s_nPendingError;
    }

    if (ppPointer == NULL) {
        CB_DEBUG(("ppPointer is null"));
        return ERROR_INVALID_HANDLE;
    }
    if (*ppPointer == NULL) {
        error = ERROR_INVALID_HANDLE;
        s_nPendingError = error;
        s_ppPendingError = ppPointer;
        CB_DEBUG((boost::format("*ppPointer is null (ppPointer=%p)")% (void*)ppPointer).str());
        return error;
    }

    PBYTE pbTarget = (PBYTE)*ppPointer;
    PDETOUR_TRAMPOLINE pTrampoline = NULL;
    DetourOperation *o = NULL;

#ifdef DETOURS_IA64
    PPLABEL_DESCRIPTOR ppldDetour = (PPLABEL_DESCRIPTOR)pDetour;
    PPLABEL_DESCRIPTOR ppldTarget = (PPLABEL_DESCRIPTOR)pbTarget;
    PVOID pDetourGlobals = NULL;
    PVOID pTargetGlobals = NULL;

    pDetour = (PBYTE)DetourCodeFromPointer(ppldDetour, &pDetourGlobals);
    pbTarget = (PBYTE)DetourCodeFromPointer(ppldTarget, &pTargetGlobals);
    CB_DEBUG((boost::format("  ppldDetour=%p, code=%p [gp=%p]")
        % (void*)ppldDetour % (void*)pDetour % (void*)pDetourGlobals
        ).str());
    CB_DEBUG((boost::format("  ppldTarget=%p, code=%p [gp=%p]")
        % (void*)ppldTarget % (void*)pbTarget % (void*)pTargetGlobals
        ).str());
#else // DETOURS_IA64
#ifdef DETOURS_ARM
    ULONG IsThumbTarget = (ULONG)pbTarget & 1;
    PVOID pGlobals = &IsThumbTarget;
#else 
    PVOID pGlobals = NULL;
#endif
    pbTarget = (PBYTE)DetourCodeFromPointer(pbTarget, &(PVOID &)pGlobals);
    pDetour = DetourCodeFromPointer(pDetour, &(PVOID &)pGlobals);
#endif // !DETOURS_IA64

    // Don't follow a jump if its destination is the target function.
    // This happens when the detour does nothing other than call the target.
    if (pDetour == (PVOID)pbTarget) {
        if (s_fIgnoreTooSmall) {
            goto stop;
        }
        else {
            goto fail;
        }
    }

    if (ppRealTarget != NULL) {
        *ppRealTarget = pbTarget;
    }
    if (ppRealDetour != NULL) {
        *ppRealDetour = pDetour;
    }

    o = new NOTHROW DetourOperation;
    if (o == NULL) {
        error = ERROR_NOT_ENOUGH_MEMORY;
    fail:
        s_nPendingError = error;
    stop:
        if (pTrampoline != NULL) {
            detour_free_trampoline(pTrampoline);
            pTrampoline = NULL;
            if (ppRealTrampoline != NULL) {
                *ppRealTrampoline = NULL;
            }
        }
        if (o != NULL) {
            delete o;
            o = NULL;
        }
        s_ppPendingError = ppPointer;
        return error;
    }

    pTrampoline = detour_alloc_trampoline(pbTarget);
    if (pTrampoline == NULL) {
        error = ERROR_NOT_ENOUGH_MEMORY;
        goto fail;
    }

    if (ppRealTrampoline != NULL) {
        *ppRealTrampoline = pTrampoline;
    }

    CB_DEBUG((boost::format("detours: pbTramp=%p, pDetour=%p")% (void*)pTrampoline % (void*)pDetour).str());

    memset(pTrampoline->rAlign, 0, sizeof(pTrampoline->rAlign));

    // Determine the number of movable target instructions.
    PBYTE pbSrc = pbTarget;
    PBYTE pbTrampoline = pTrampoline->rbCode;
#ifdef DETOURS_IA64
    PBYTE pbPool = (PBYTE)(&pTrampoline->bBranchIslands + 1);
#else
    PBYTE pbPool = pbTrampoline + sizeof(pTrampoline->rbCode);
#endif
    ULONG cbTarget = 0;
    ULONG cbJump = SIZE_OF_JMP;
    ULONG nAlign = 0;

#ifdef DETOURS_ARM

    // On ARM, we need an extra instruction when the function isn't 32-bit aligned.
    // Check if the existing code is another detour (or at least a similar
    // "ldr pc, [PC+0]" jump.
    pTrampoline->IsThumbTarget = IsThumbTarget;

    if ((ULONG)pbTarget & 2) {
        cbJump += 2;

        ULONG op = fetch_thumb_opcode(pbSrc);
        if (op == 0xbf00) {
            op = fetch_t.c_str()humb_opcode(pbSrc + 2);
            if (op == 0xf8dff000) { // LDR PC,[PC]
                *((PUSHORT&)pbTrampoline)++ = *((PUSHORT&)pbSrc)++;
                *((PULONG&)pbTrampoline)++ = *((PULONG&)pbSrc)++;
                *((PULONG&)pbTrampoline)++ = *((PULONG&)pbSrc)++;
                cbTarget = (LONG)(pbSrc - pbTarget);
                // We will fall through the "while" because cbTarget is now >= cbJump.
            }
        }
    }
    else {
        ULONG op = fetch_thumb_opcode(pbSrc);
        if (op == 0xf8dff000) { // LDR PC,[PC]
            *((PULONG&)pbTrampoline)++ = *((PULONG&)pbSrc)++;
            *((PULONG&)pbTrampoline)++ = *((PULONG&)pbSrc)++;
            cbTarget = (LONG)(pbSrc - pbTarget);
            // We will fall through the "while" because cbTarget is now >= cbJump.
        }
    }
#endif

    std::string pbTrampoline_code = dumpHex((const char*)pbTrampoline, 32);
    std::string pbSrc_code = dumpHex((const char*)pbSrc, 32);
    std::string pDetour_code = dumpHex((const char*)pDetour, 32);
    CB_DEBUG((boost::format("pbTrampoline code(%p):\n%s")% (void*)pbTrampoline% pbTrampoline_code.c_str()).str());
    CB_DEBUG((boost::format("pbSrc code(%p):\n%s")% (void*)pbSrc% pbSrc_code.c_str()).str());
    CB_DEBUG((boost::format("pDetour code(%p):\n%s")% (void*)pDetour% pDetour_code.c_str()).str());

#ifdef DETOURS_MIPS64
    auto CopyNInstruction = [](PVOID pDst, PVOID pSrc, size_t ins_size) -> PVOID
    {
        int ins_len = 4;
        int ins_bytes = ins_size * ins_len;
        memcpy(pDst, pSrc, sizeof(char) * ins_bytes);

        return pSrc + ins_bytes;
    };
#endif

    while (cbTarget < cbJump) {
        PBYTE pbOp = pbSrc;
#ifdef DETOURS_ARM
        LONG lExtra = IsThumbTarget;
#else
        LONG lExtra = NULL;
#endif
        CB_DEBUG((boost::format(" DetourCopyInstruction(%p,%p)")
            % (void*)pbTrampoline % (void*)pbSrc
            ).str());
        pbSrc = (PBYTE)
#ifndef DETOURS_MIPS64
            DetourCopyInstruction(pbTrampoline, (PVOID*)&pbPool, pbSrc, NULL, &lExtra);
#else
            CopyNInstruction(pbTrampoline, pbSrc, 2);
#endif
        CB_DEBUG((boost::format(" DetourCopyInstruction() = %p (%d bytes)")
            % (void*)pbSrc % (int)(pbSrc - pbOp)
            ).str());
        pbTrampoline += (pbSrc - pbOp) + lExtra;
        cbTarget = (LONG)(pbSrc - pbTarget);
        pTrampoline->rAlign[nAlign].obTarget = cbTarget;
        pTrampoline->rAlign[nAlign].obTrampoline = pbTrampoline - pTrampoline->rbCode;
        nAlign++;

        std::string rbCode = dumpHex((const char*)pTrampoline->rbCode, 32);
        CB_DEBUG((boost::format("rbCode(%p):\n%s")
                 % (void*)pTrampoline->rbCode % rbCode.c_str()
                 ).str());

        if (nAlign >= ARRAYSIZE(pTrampoline->rAlign)) {
            break;
        }

        if (detour_does_code_end_function(pbOp)) {
            break;
        }
    }

    // Consume, but don't duplicate padding if it is needed and available.
    while (cbTarget < cbJump) {
        LONG cFiller = detour_is_code_filler(pbSrc);
        if (cFiller == 0) {
            break;
        }

        pbSrc += cFiller;
        cbTarget = (LONG)(pbSrc - pbTarget);
    }

#if 0
    {
        CB_DEBUG((" detours: rAlign ["));
        LONG n = 0;
        for (n = 0; n < ARRAYSIZE(pTrampoline->rAlign); n++) {
            if (pTrampoline->rAlign[n].obTarget == 0 &&
                pTrampoline->rAlign[n].obTrampoline == 0) {
                break;
            }
            CB_DEBUG((boost::format(" %d/%d")
                % (uint8_t)pTrampoline->rAlign[n].obTarget
                % (uint8_t)pTrampoline->rAlign[n].obTrampoline
                ).str());
        }
        CB_DEBUG((" ]"));
    }
#endif

#if DETOUR_DEBUG
    {
        CB_DEBUG((" detours: rAlign ["));
        LONG n = 0;
        for (n = 0; n < ARRAYSIZE(pTrampoline->rAlign); n++) {
            if (pTrampoline->rAlign[n].obTarget == 0 &&
                pTrampoline->rAlign[n].obTrampoline == 0) {
                break;
            }
            CB_DEBUG((boost::format(" %d/%d")
                % pTrampoline->rAlign[n].obTarget
                % pTrampoline->rAlign[n].obTrampoline
                ).str());
        }
        CB_DEBUG((" ]"));
    }
#endif

    if (cbTarget < cbJump || nAlign > ARRAYSIZE(pTrampoline->rAlign)) {
        // Too few instructions.

        error = ERROR_INVALID_BLOCK;
        if (s_fIgnoreTooSmall) {
            goto stop;
        }
        else {
            goto fail;
        }
    }

    if (pbTrampoline > pbPool) {
        __debugbreak();
    }

    pTrampoline->cbCode = (BYTE)(pbTrampoline - pTrampoline->rbCode);
    pTrampoline->cbRestore = (BYTE)cbTarget;
    CopyMemory(pTrampoline->rbRestore, pbTarget, cbTarget);

#if !defined(DETOURS_IA64)
    if (cbTarget > sizeof(pTrampoline->rbCode) - cbJump) {
        // Too many instructions.
        error = ERROR_INVALID_HANDLE;
        goto fail;
    }
#endif // !DETOURS_IA64

//#ifndef DETOURS_MIPS64
    pTrampoline->pbRemain = pbTarget + cbTarget;
//#else
    //pTrampoline->pbRemain = pbTarget + cbTarget + 4; // 4 byte is nop, below j(jmp) instruction
//#endif
    pTrampoline->pbDetour = (PBYTE)pDetour;

    InsertTraceHandle(pTrampoline);

#ifdef DETOURS_IA64
    pTrampoline->ppldDetour = ppldDetour;
    pTrampoline->ppldTarget = ppldTarget;
    pTrampoline->pldTrampoline.EntryPoint = (UINT64)&pTrampoline->bMovlTargetGp;
    pTrampoline->pldTrampoline.GlobalPointer = (UINT64)pDetourGlobals;

    ((DETOUR_IA64_BUNDLE *)pTrampoline->rbCode)->SetStop();

    pTrampoline->bMovlTargetGp.SetMovlGp((UINT64)pTargetGlobals);
    pTrampoline->bBrlRemainEip.SetBrl((UINT64)pTrampoline->pbRemain);

    // Alloc frame:      alloc r41=ar.pfs,11,0,8,0; mov r40=rp
    pTrampoline->bAllocFrame.wide[0] = 0x00000580164d480c;
    pTrampoline->bAllocFrame.wide[1] = 0x00c4000500000200;
    // save r36, r37, r38.
    pTrampoline->bSave37to39.wide[0] = 0x031021004e019001;
    pTrampoline->bSave37to39.wide[1] = 0x8401280600420098;
    // save r34,r35,r36: adds r47=0,r36; adds r46=0,r35; adds r45=0,r34
    pTrampoline->bSave34to36.wide[0] = 0x02e0210048017800;
    pTrampoline->bSave34to36.wide[1] = 0x84011005a042008c;
    // save gp,r32,r33"  adds r44=0,r33; adds r43=0,r32; adds r42=0,gp ;;
    pTrampoline->bSaveGPto33.wide[0] = 0x02b0210042016001;
    pTrampoline->bSaveGPto33.wide[1] = 0x8400080540420080;
    // set detour GP.
    pTrampoline->bMovlDetourGp.SetMovlGp((UINT64)pDetourGlobals);
    // call detour:      brl.call.sptk.few rp=detour ;;
    pTrampoline->bCallDetour.wide[0] = 0x0000000100000005;
    pTrampoline->bCallDetour.wide[1] = 0xd000001000000000;
    pTrampoline->bCallDetour.SetBrlTarget((UINT64)pDetour);
    // pop frame & gp:   adds gp=0,r42; mov rp=r40,+0;; mov.i ar.pfs=r41
    pTrampoline->bPopFrameGp.wide[0] = 0x4000210054000802;
    pTrampoline->bPopFrameGp.wide[1] = 0x00aa029000038005;
    // return to caller: br.ret.sptk.many rp ;;
    pTrampoline->bReturn.wide[0] = 0x0000000100000019;
    pTrampoline->bReturn.wide[1] = 0x0084000880000200;

    CB_DEBUG((boost::format("detours: &bMovlTargetGp=%p")% &pTrampoline->bMovlTargetGp).str());
    CB_DEBUG((boost::format("detours: &bMovlDetourGp=%p")% &pTrampoline->bMovlDetourGp).str());
#endif // DETOURS_IA64

    pbTrampoline = pTrampoline->rbCode + pTrampoline->cbCode;
#ifdef DETOURS_X64
    pbTrampoline = detour_gen_jmp_indirect(pbTrampoline, &pTrampoline->pbRemain);
    pbTrampoline = detour_gen_brk(pbTrampoline, pbPool);
#endif // DETOURS_X64

#ifdef DETOURS_X86
    pbTrampoline = detour_gen_jmp_immediate(pbTrampoline, pTrampoline->pbRemain);
    pbTrampoline = detour_gen_brk(pbTrampoline, pbPool);
#endif // DETOURS_X86

#ifdef DETOURS_MIPS64
    pbTrampoline = detour_gen_jmp_immediate(pbTrampoline, pTrampoline->pbRemain);
    //pbTrampoline = detour_gen_jr_ra_inc(pbTrampoline);
    pbTrampoline = detour_gen_brk(pbTrampoline, pbPool);
#endif // DETOURS_MIPS64

#ifdef DETOURS_ARM
    pbTrampoline = detour_gen_jmp_immediate(pbTrampoline + pTrampoline->IsThumbTarget, &pbPool, pTrampoline->pbRemain);
    pbTrampoline = detour_gen_brk(pbTrampoline, pbPool);
#endif // DETOURS_ARM

#ifdef DETOURS_ARM64
    pbTrampoline = detour_gen_jmp_immediate(pbTrampoline, &pbPool, pTrampoline->pbRemain);
    pbTrampoline = detour_gen_brk(pbTrampoline, pbPool);
#endif // DETOURS_ARM64

    (void)pbTrampoline;
    std::string trampoline_string = dumpTrampoline(pTrampoline);
    CB_DEBUG((boost::format("dump trampoline:%s")% trampoline_string.c_str()).str());

    DWORD dwOld = PAGE_EXECUTE_READ;

    if (mprotect(detour_get_page(pbTarget), detour_get_page_size(), PAGE_EXECUTE_READWRITE)) {
        error = -1;
        goto fail;
    }


    CB_DEBUG((boost::format("detours: pbTarget=%p: "
        "%02x %02x %02x %02x "
        "%02x %02x %02x %02x "
        "%02x %02x %02x %02x")
        % (void*)pbTarget
        % (int)pbTarget[0] % (int)pbTarget[1] % (int)pbTarget[2] % (int)pbTarget[3]
        % (int)pbTarget[4] % (int)pbTarget[5] % (int)pbTarget[6] % (int)pbTarget[7]
        % (int)pbTarget[8] % (int)pbTarget[9] % (int)pbTarget[10] % (int)pbTarget[11]
        ).str());
    CB_DEBUG((boost::format("detours: pbTramp =%p: "
        "%02x %02x %02x %02x "
        "%02x %02x %02x %02x "
        "%02x %02x %02x %02x")
        % (void*)pTrampoline
        % (int)pTrampoline->rbCode[0] % (int)pTrampoline->rbCode[1]
        % (int)pTrampoline->rbCode[2] % (int)pTrampoline->rbCode[3]
        % (int)pTrampoline->rbCode[4] % (int)pTrampoline->rbCode[5]
        % (int)pTrampoline->rbCode[6] % (int)pTrampoline->rbCode[7]
        % (int)pTrampoline->rbCode[8] % (int)pTrampoline->rbCode[9]
        % (int)pTrampoline->rbCode[10] % (int)pTrampoline->rbCode[11]
        ).str());

    o->fIsRemove = FALSE;
    o->ppbPointer = (PBYTE*)ppPointer;
    o->pTrampoline = pTrampoline;
    o->pbTarget = pbTarget;
    o->dwPerm = dwOld;
    o->pNext = s_pPendingOperations;
    s_pPendingOperations = o;

    return NO_ERROR;
}

LONG DetourDetach(_Inout_ PVOID *ppPointer,
    _In_ PVOID pDetour)
{
    LONG error = NO_ERROR;

    if (s_nPendingThreadId != (LONG)pthread_self()) {
        return ERROR_INVALID_OPERATION;
    }

    // If any of the pending operations failed, then we don't need to do this.
    if (s_nPendingError != NO_ERROR) {
        return s_nPendingError;
    }

    if (pDetour == NULL) {
        return ERROR_INVALID_PARAMETER;
    }
    if (ppPointer == NULL) {
        return ERROR_INVALID_HANDLE;
    }
    if (*ppPointer == NULL) {
        error = ERROR_INVALID_HANDLE;
        s_nPendingError = error;
        s_ppPendingError = ppPointer;
        return error;
    }

    DetourOperation *o = new NOTHROW DetourOperation;
    if (o == NULL) {
        error = ERROR_NOT_ENOUGH_MEMORY;
    fail:
        s_nPendingError = error;
    stop:
        if (o != NULL) {
            delete o;
            o = NULL;
        }
        s_ppPendingError = ppPointer;
        return error;
    }


#ifdef DETOURS_IA64
    PPLABEL_DESCRIPTOR ppldTrampo = (PPLABEL_DESCRIPTOR)*ppPointer;
    PPLABEL_DESCRIPTOR ppldDetour = (PPLABEL_DESCRIPTOR)pDetour;
    PVOID pDetourGlobals = NULL;
    PVOID pTrampoGlobals = NULL;

    pDetour = (PBYTE)DetourCodeFromPointer(ppldDetour, &pDetourGlobals);
    PDETOUR_TRAMPOLINE pTrampoline = (PDETOUR_TRAMPOLINE)
        DetourCodeFromPointer(ppldTrampo, &pTrampoGlobals);
    CB_DEBUG((boost::format("  ppldDetour=%p, code=%p [gp=%p]")
         % (void*)ppldDetour % (void*)pDetour % (void*)pDetourGlobals
         ).str());
    CB_DEBUG((boost::format("  ppldTrampo=%p, code=%p [gp=%p]")
         % (void*)ppldTrampo % (void*)pTrampoline % (void*)pTrampoGlobals
         ).str());

    CB_DEBUG((""));
    CB_DEBUG((boost::format("detours:  &pldTrampoline  =%p")
         % &pTrampoline->pldTrampoline
         ).str());
    CB_DEBUG((boost::format("detours:  &bMovlTargetGp  =%p [%p]")
        % &pTrampoline->bMovlTargetGp
        % (void*)pTrampoline->bMovlTargetGp.GetMovlGp()
        ).str());
    CB_DEBUG((boost::format("detours:  &rbCode         =%p [%p]")
        % &pTrampoline->rbCode
        % ((DETOUR_IA64_BUNDLE&)pTrampoline->rbCode).GetBrlTarget()
        ).str());
    CB_DEBUG((boost::format("detours:  &bBrlRemainEip  =%p [%p]")
        % &pTrampoline->bBrlRemainEip
        % (void*)pTrampoline->bBrlRemainEip.GetBrlTarget()
        ).str());
    CB_DEBUG((boost::format("detours:  &bMovlDetourGp  =%p [%p]")
        % &pTrampoline->bMovlDetourGp
        % (void*)pTrampoline->bMovlDetourGp.GetMovlGp()
        ).str());
    CB_DEBUG((boost::format("detours:  &bBrlDetourEip  =%p [%p]")
        % &pTrampoline->bCallDetour
        % (void*)pTrampoline->bCallDetour.GetBrlTarget()
        ).str());
    CB_DEBUG((boost::format("detours:  pldDetour       =%p [%p]")
        % pTrampoline->ppldDetour->EntryPoint
        % pTrampoline->ppldDetour->GlobalPointer()
        ).str());
    CB_DEBUG((boost::format("detours:  pldTarget       =%p [%p]")
        % pTrampoline->ppldTarget->EntryPoint
        % pTrampoline->ppldTarget->GlobalPointer()
        ).str());
    CB_DEBUG((boost::format("detours:  pbRemain        =%p")
        % (void*)pTrampoline->pbRemain
        ).str());
    CB_DEBUG((boost::format("detours:  pbDetour        =%p")
        % (void*)pTrampoline->pbDetour
        ).str());
    CB_DEBUG((""));
#else // !DETOURS_IA64
    PDETOUR_TRAMPOLINE pTrampoline =
        (PDETOUR_TRAMPOLINE)DetourCodeFromPointer(*ppPointer, NULL);
    pDetour = DetourCodeFromPointer(pDetour, NULL);
#endif // !DETOURS_IA64

    ////////////////////////////////////// Verify that Trampoline is in place.
    //
    LONG cbTarget = pTrampoline->cbRestore;
    PBYTE pbTarget = pTrampoline->pbRemain - cbTarget;
    if (cbTarget == 0 || cbTarget > sizeof(pTrampoline->rbCode)) {
        error = ERROR_INVALID_BLOCK;
        if (s_fIgnoreTooSmall) {
            goto stop;
        }
        else {
            goto fail;
        }
    }

    if (pTrampoline->pbDetour != pDetour) {
        error = ERROR_INVALID_BLOCK;
        if (s_fIgnoreTooSmall) {
            goto stop;
        }
        else {
            goto fail;
        }
    }
    if (mprotect(detour_get_page(pbTarget), detour_get_page_size(), PAGE_EXECUTE_READWRITE)) {
        error = -1;
        goto fail;
    }

    o->fIsRemove = TRUE;
    o->ppbPointer = (PBYTE*)ppPointer;
    o->pTrampoline = pTrampoline;
    o->pbTarget = pbTarget;
    o->dwPerm = 0;
    o->pNext = s_pPendingOperations;
    s_pPendingOperations = o;

    return NO_ERROR;
}

//////////////////////////////////////////////////////////////////////////////
//
// Helpers for manipulating page protection.
//

// For reference:
//   PAGE_NOACCESS          0x01
//   PAGE_READONLY          0x02
//   PAGE_READWRITE         0x04
//   PAGE_WRITECOPY         0x08
//   PAGE_EXECUTE           0x10
//   PAGE_EXECUTE_READ      0x20
//   PAGE_EXECUTE_READWRITE 0x40
//   PAGE_EXECUTE_WRITECOPY 0x80
//   PAGE_GUARD             ...
//   PAGE_NOCACHE           ...
//   PAGE_WRITECOMBINE      ...

#define DETOUR_PAGE_EXECUTE_ALL    (PROT_EXEC |              \
                                    PROT_READ |         \
                                    PROT_WRITE )

#define DETOUR_PAGE_NO_EXECUTE_ALL (PROT_READ |             \
                                    PROT_WRITE )

#define DETOUR_PAGE_ATTRIBUTES     (~(DETOUR_PAGE_EXECUTE_ALL | DETOUR_PAGE_NO_EXECUTE_ALL))

//C_ASSERT((DETOUR_PAGE_NO_EXECUTE_ALL << 4) == DETOUR_PAGE_EXECUTE_ALL);

static DWORD DetourPageProtectAdjustExecute(_In_  DWORD dwOldProtect,
    _In_  DWORD dwNewProtect)
    //  Copy EXECUTE from dwOldProtect to dwNewProtect.
{
    bool const fOldExecute = ((dwOldProtect & DETOUR_PAGE_EXECUTE_ALL) != 0);
    bool const fNewExecute = ((dwNewProtect & DETOUR_PAGE_EXECUTE_ALL) != 0);

    if (fOldExecute && !fNewExecute) {
        dwNewProtect = ((dwNewProtect & DETOUR_PAGE_NO_EXECUTE_ALL) << 4)
            | (dwNewProtect & DETOUR_PAGE_ATTRIBUTES);
    }
    else if (!fOldExecute && fNewExecute) {
        dwNewProtect = ((dwNewProtect & DETOUR_PAGE_EXECUTE_ALL) >> 4)
            | (dwNewProtect & DETOUR_PAGE_ATTRIBUTES);
    }
    return dwNewProtect;
}

_Success_(return != FALSE)
BOOL DetourVirtualProtectSameExecuteEx(_In_  pid_t hProcess,
    _In_  PVOID pAddress,
    _In_  SIZE_T nSize,
    _In_  DWORD dwNewProtect,
    _Out_ PDWORD pdwOldProtect)
    // Some systems do not allow executability of a page to change. This function applies
    // dwNewProtect to [pAddress, nSize), but preserving the previous executability.
    // This function is meant to be a drop-in replacement for some uses of VirtualProtectEx.
    // When "restoring" page protection, there is no need to use this function.
{
    return TRUE;
    /*
    MEMORY_BASIC_INFORMATION mbi;

    // Query to get existing execute access.

    ZeroMemory(&mbi, sizeof(mbi));

    if (VirtualQueryEx(hProcess, pAddress, &mbi, sizeof(mbi)) == 0) {
    return FALSE;
    }
    return VirtualProtectEx(hProcess, pAddress, nSize,
    DetourPageProtectAdjustExecute(mbi.Protect, dwNewProtect),
    pdwOldProtect);
    */
}

_Success_(return != FALSE)
BOOL DetourVirtualProtectSameExecute(_In_  PVOID pAddress,
    _In_  SIZE_T nSize,
    _In_  DWORD dwNewProtect,
    _Out_ PDWORD pdwOldProtect)
{
    return DetourVirtualProtectSameExecuteEx(getpid(),
                                           pAddress, nSize, dwNewProtect, pdwOldProtect);
}
void library_entry_point(void) __attribute__((constructor));

void library_entry_point()
{
    DetourBarrierProcessAttach();
    DetourCriticalInitialize();
}
__attribute__((destructor))
void library_exit()
{
    DetourCriticalFinalize();    
    DetourBarrierProcessDetach();
}

std::string dumpTrampoline(const _DETOUR_TRAMPOLINE* trampoline)
{
    std::ostringstream oss;

    oss << "\n\n";
    oss << "-------------------- dump begin dumpTrampoline --------------------" << std::endl;
    oss << "trampoline address: " << trampoline << std::endl;
    oss << "\nrbCode(" << std::hex << (void*)trampoline->rbCode << "), size: " << std::dec << sizeof(trampoline->rbCode) << " "
            << "(target code + jmp to pbRemain.) "
            << "hex dump:\n" << dumpHex((const char*)trampoline->rbCode, sizeof(trampoline->rbCode)) << "\n\n"

        << "cbCode: " << std::hex << (void*)trampoline->cbCode << " \n"
        << "cbCodeBreak: " << std::hex << (void*)trampoline->cbCodeBreak << " \n\n"

        << "rbRestore(" << std::hex << (void*)trampoline->rbRestore << "), size: " << std::dec << sizeof(trampoline->rbRestore) << " "
            << "(original target code.) "
            << "hex dump:\n" << dumpHex((const char*)trampoline->rbRestore, sizeof(trampoline->rbRestore)) << "\n\n"

        << "cbRestore: " << std::hex << (void*)trampoline->cbRestore << " \n"
        << "cbRestoreBreak: " << std::hex << (void*)trampoline->cbRestoreBreak << " \n\n"

        << "pbRemain: " << std::hex << (void*)trampoline->pbRemain << " \n"
            << "(first instruction after moved code. [free list]) "
            << "hex dump:\n" << dumpHex((const char*)trampoline->pbRemain, 32) << "\n"

        << "pbDetour: " << std::hex << (void*)trampoline->pbDetour << " \n"
            << "(first instruction of detour function.) "
            << "hex dump:\n" << dumpHex((const char*)trampoline->pbDetour, 32) << "\n"

        << "rbCodeIn(" << std::hex << (void*)trampoline->rbCodeIn << "), size: " << std::dec << sizeof(trampoline->rbCodeIn) << " "
            << "(jmp [pbDetour])"
            << "hex dump:\n" << dumpHex((const char*)trampoline->rbCodeIn, sizeof(trampoline->rbCodeIn)) << "\n\n"

        << "Callback: " << std::hex << trampoline->Callback << " \n"
        << "Trampoline: " << std::hex << trampoline->Trampoline << " \n"
        << "HookIntro: " << std::hex << trampoline->HookIntro << " \n"
        << "OldProc: " << std::hex << (void*)trampoline->OldProc << " \n"
        << "HookProc: " << std::hex << trampoline->HookProc << " \n"
        << "HookOutro: " << std::hex << trampoline->HookOutro << " \n"
        << "IsExecutedPtr: " << std::hex << trampoline->IsExecutedPtr << " \n\n"

        << "rbTrampolineCode(" << std::hex << (void*)trampoline->rbTrampolineCode << "), size: "
            << std::dec << DETOUR_TRAMPOLINE_CODE_SIZE << " "
            << "hex dump:\n" << dumpHex((const char*)trampoline->rbTrampolineCode, DETOUR_TRAMPOLINE_CODE_SIZE) << "\n"
        ;
    oss << "-------------------- dump end dumpTrampoline --------------------" << std::endl;

    std::cerr << oss.str() << std::endl;
    return oss.str();
}

//  End of File

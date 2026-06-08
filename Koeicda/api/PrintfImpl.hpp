#pragma once
#include "pch.h"
#include <intrin.h>
#include "../util/HookManager.hpp"
#include "../util/AobScan.hpp"
#ifdef _DEBUG
#include "../util/DebugLog.hpp"
#endif

//*****************************************************
// PrintfImpl (遊戲內部函數, 特徵碼掃描定位)
//   核心 printf 實作, 負責格式化所有遊戲內部的變長參數字串。
//   [特徵]
//     55 8B EC 83 EC 64           : push ebp; mov ebp,esp; sub esp,64h
//     C7 45 FC ?? ?? ?? ??        : mov [ebp-4], <abs_addr> (此位址隨版本變動)
//     8B 45 08 8B 08 89 4D BC     : eax=pArgs; ecx=*pArgs(fmt); [ebp-44h]=fmt
//     8B 55 08 83 C2 04 89 55 08  : pArgs+=4 (指向 va_list); [ebp+8]=pArgs
//*****************************************************

#define PRINTF_IMPL_RVA 0x0000E56B
static const unsigned char kPrintfSig[] = {
    0x55,0x8B,0xEC,0x83,0xEC,0x64,0xC7,0x45,0xFC, 0x00,0x00,0x00,0x00,
    0x8B,0x45,0x08,0x8B,0x08,0x89,0x4D,0xBC,
    0x8B,0x55,0x08,0x83,0xC2,0x04,0x89,0x55,0x08,
    0x8B,0x45,0x08,0x89,0x45,0xB8
};
static const char kPrintfMask[] = "xxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxx";

typedef int (__cdecl *PrintfImpl_t)(void** pArgs);

extern "C" {
    int __cdecl MyPrintfImpl(void** pArgs);
}

static PVOID g_PrintfImplAddr = nullptr;
static HookManager* g_PrintfImplHook = nullptr;

int __cdecl MyPrintfImpl(void** pArgs)
{
#ifdef _DEBUG
    PVOID callerAddr = _ReturnAddress();
    DWORD rva = (DWORD)((BYTE*)callerAddr - (BYTE*)GetModuleHandleW(nullptr));
    char* fmt = (pArgs != nullptr) ? (char*)pArgs[0] : nullptr;
    if (!strcmp(fmt, "%s"))
        DebugLog("[PrintfImpl] rva=0x%08X fmt=%s pArgs=%s\n", rva, fmt ? fmt : "null", (char *)pArgs[1] ? (char *)pArgs[1] : "null");
    else
        DebugLog("[PrintfImpl] rva=0x%08X fmt=%s\n", rva, fmt ? fmt : "null");
#endif

    g_PrintfImplHook->unhook();
    int ret = ((PrintfImpl_t)g_PrintfImplAddr)(pArgs);
    g_PrintfImplHook->hook();

    return ret;
}

inline void Install_PrintfImpl_Hook()
{
    g_PrintfImplAddr = FindAddress(kPrintfSig, kPrintfMask, PRINTF_IMPL_RVA);
    if (g_PrintfImplAddr) {
        g_PrintfImplHook = new HookManager(g_PrintfImplAddr, MyPrintfImpl);
        g_PrintfImplHook->hook();
    }

#ifdef _DEBUG
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (g_PrintfImplAddr == nullptr)
        DebugLog("[PrintfImpl] Feature Code scan failed, not hook\n");
    else
        DebugLog("[PrintfImpl] addr=%p rva=0x%X %s\n",
            g_PrintfImplAddr, (unsigned)((BYTE*)g_PrintfImplAddr - base),
            ((BYTE*)g_PrintfImplAddr == base + PRINTF_IMPL_RVA) ? "(by fixed RVA)" : "(found by scan)");
#endif
}

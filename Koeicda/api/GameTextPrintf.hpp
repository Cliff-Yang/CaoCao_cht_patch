#pragma once
#include "pch.h"
#include <intrin.h>
#include "../util/HookManager.hpp"
#include "../util/AobScan.hpp"
#include "PrintfImpl.hpp"   // 取用 g_PrintfImplAddr
#ifdef _DEBUG
#include "../util/DebugLog.hpp"
#endif

//*****************************************************
// GameTextPrintf (遊戲內部函數, 特徵碼掃描定位)
//   遊戲繪圖引擎的文字輸出包裝函數, 對話框逐字顯示時會頻繁呼叫。
//   [特徵]
//     55 8B EC                    : push ebp; mov ebp,esp
//     E8 ?? ?? ?? ??              : call <sync_state_func> (相對位址隨版本變動)
//     8D 45 0C 50                 : lea eax,[ebp+0Ch]; push eax (取得 fmt 的位址)
//     E8 ?? ?? ?? ??              : call <printf_impl> (相對位址隨版本變動)
//*****************************************************

#define GAME_TEXT_PRINTF_RVA 0x0000FAA0
static const unsigned char kGameTextPrintfSig[] = {
    0x55, 0x8B, 0xEC, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x8D, 0x45, 0x0C, 0x50, 0xE8, 0x00, 0x00, 0x00, 0x00
};
static const char kGameTextPrintfMask[] = "xxxx????xxxxx????";

typedef int(__cdecl* GameTextPrintf_t)(void* arg1, const char* fmt, ...);

extern "C" {
    int __cdecl MyGameTextPrintf(void* arg1, const char* fmt, ...);
}

static PVOID g_GameTextPrintfAddr = nullptr;
static HookManager* g_GameTextPrintfHook = nullptr;

int __cdecl MyGameTextPrintf(void* arg1, const char* fmt, ...)
{
#ifdef _DEBUG
    PVOID callerAddr = _ReturnAddress();
    DWORD rva = (DWORD)((BYTE*)callerAddr - (BYTE*)GetModuleHandleW(nullptr));

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf_s(buf, fmt, args);
    va_end(args);

    DebugLog("[GameTextPrintf @ RVA 0x%08X] %s\n", rva, buf);
#endif

    g_GameTextPrintfHook->unhook();
    // 使用 printf_impl 轉發以確保參數正確
    typedef int(__cdecl* printf_impl_t)(void**);
    int ret = ((printf_impl_t)g_PrintfImplAddr)((void**)&fmt);
    g_GameTextPrintfHook->hook();

    return ret;
}

inline void Install_GameTextPrintf_Hook()
{
    g_GameTextPrintfAddr = FindAddress(kGameTextPrintfSig, kGameTextPrintfMask, GAME_TEXT_PRINTF_RVA);
    if (g_GameTextPrintfAddr) {
        g_GameTextPrintfHook = new HookManager(g_GameTextPrintfAddr, MyGameTextPrintf);
        g_GameTextPrintfHook->hook();
    }

#ifdef _DEBUG
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (g_GameTextPrintfAddr == nullptr)
        DebugLog("[GameTextPrintf] 特徵碼掃描失敗, 未 hook\n");
    else
        DebugLog("[GameTextPrintf] addr=%p rva=0x%X %s\n",
            g_GameTextPrintfAddr, (unsigned)((BYTE*)g_GameTextPrintfAddr - base),
            ((BYTE*)g_GameTextPrintfAddr == base + GAME_TEXT_PRINTF_RVA) ? "(寫死 RVA 命中)" : "(掃描找到)");
#endif
}

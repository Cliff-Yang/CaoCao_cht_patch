// dllmain.cpp : 定義 DLL 應用程式的進入點。
#include "pch.h"
#include <iostream>
#include "HookManager.hpp"
#include "ChsToCht.hpp"

#include "usp10.h"
#pragma comment(lib, "Usp10.lib")

#include <mciapi.h>
#include <digitalv.h>
#include <intrin.h>
#pragma comment(lib, "Winmm.lib")

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 導出函數
#pragma comment(linker, "/EXPORT:CDAudioClose=Koeicda_Origin.CDAudioClose,@1")
#pragma comment(linker, "/EXPORT:CDAudioEjectMedia=Koeicda_Origin.CDAudioEjectMedia,@2")
#pragma comment(linker, "/EXPORT:CDAudioGetDriveLetter=Koeicda_Origin.CDAudioGetDriveLetter,@3")
#pragma comment(linker, "/EXPORT:CDAudioGetNumDrives=Koeicda_Origin.CDAudioGetNumDrives,@4")
#pragma comment(linker, "/EXPORT:CDAudioGetNumTracks=Koeicda_Origin.CDAudioGetNumTracks,@5")
#pragma comment(linker, "/EXPORT:CDAudioGetPlayingPos=Koeicda_Origin.CDAudioGetPlayingPos,@6")
#pragma comment(linker, "/EXPORT:CDAudioGetStartPos=Koeicda_Origin.CDAudioGetStartPos,@7")
#pragma comment(linker, "/EXPORT:CDAudioGetTrackLength=Koeicda_Origin.CDAudioGetTrackLength,@8")
#pragma comment(linker, "/EXPORT:CDAudioInitialize=Koeicda_Origin.CDAudioInitialize,@9")
#pragma comment(linker, "/EXPORT:CDAudioIsInserted=Koeicda_Origin.CDAudioIsInserted,@10")
#pragma comment(linker, "/EXPORT:CDAudioIsPlaying=Koeicda_Origin.CDAudioIsPlaying,@11")
#pragma comment(linker, "/EXPORT:CDAudioNextPlay=Koeicda_Origin.CDAudioNextPlay,@12")
#pragma comment(linker, "/EXPORT:CDAudioNextPlayTrack=Koeicda_Origin.CDAudioNextPlayTrack,@13")
#pragma comment(linker, "/EXPORT:CDAudioOpen=Koeicda_Origin.CDAudioOpen,@14")
#pragma comment(linker, "/EXPORT:CDAudioPause=Koeicda_Origin.CDAudioPause,@15")
#pragma comment(linker, "/EXPORT:CDAudioPlay=Koeicda_Origin.CDAudioPlay,@16")
#pragma comment(linker, "/EXPORT:CDAudioPlayTrack=Koeicda_Origin.CDAudioPlayTrack,@17")
#pragma comment(linker, "/EXPORT:CDAudioResume=Koeicda_Origin.CDAudioResume,@18")
#pragma comment(linker, "/EXPORT:CDAudioStop=Koeicda_Origin.CDAudioStop,@19")
#pragma comment(linker, "/EXPORT:CDAudioTerminate=Koeicda_Origin.CDAudioTerminate,@20")
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOG_FILE "log.txt"

//*****************************************************
// MciSendCommandA
//*****************************************************

extern "C" {
    MCIERROR WINAPI MyMciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam);
}

static HookManager MciSendCommandA_HookManager {
    getLibraryProcAddress(L"winmm.dll", "mciSendCommandA"),
    MyMciSendCommandA
};

#define MAGIC_DEVICE_ID 0xBEEF

MCIERROR WINAPI MyMciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    static MCIDEVICEID s_device_id;

    switch (uMsg) {
    case MCI_OPEN:
    {
        LPMCI_OPEN_PARMSA parms = (LPMCI_OPEN_PARMSA)dwParam;
        bool cond1 = fdwCommand == MCI_OPEN_TYPE && strcmp(parms->lpstrDeviceType, "cdaudio") == 0;
        bool cond2 = (fdwCommand & MCI_OPEN_TYPE_ID) && LOWORD(parms->lpstrDeviceType) == MCI_DEVTYPE_CD_AUDIO;
        if (cond1 || cond2) {
            parms->wDeviceID = MAGIC_DEVICE_ID;
            return 0;
        }
        break;
    }

    case MCI_SET:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            MciSendCommandA_HookManager.unhook();
            mciSendCommandA(s_device_id, uMsg, fdwCommand, dwParam);
            MciSendCommandA_HookManager.hook();
            return 0;
        }
        break;
    }

    case MCI_STATUS:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            MciSendCommandA_HookManager.unhook();
            mciSendCommandA(s_device_id, uMsg, fdwCommand, dwParam);
            MciSendCommandA_HookManager.hook();
            return 0;
        }
        break;
    }

    case MCI_PLAY:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            LPMCI_PLAY_PARMS play_param = (LPMCI_PLAY_PARMS)dwParam;
            int track_number = play_param->dwFrom & 0xFF;
            char path[MAX_PATH];
            sprintf_s(path, "music\\%02d.mp3", track_number);

            MCI_OPEN_PARMSA open_param = { 0 };
            open_param.lpstrElementName = path;

            MciSendCommandA_HookManager.unhook();
            mciSendCommandA(NULL, MCI_OPEN, MCI_OPEN_ELEMENT, (DWORD_PTR)&open_param);

            s_device_id = open_param.wDeviceID;
            play_param->dwFrom = 0;

            mciSendCommandA(s_device_id, MCI_PLAY, MCI_NOTIFY | MCI_DGV_PLAY_REPEAT, (DWORD_PTR)play_param);
            MciSendCommandA_HookManager.hook();
            return 0;
        }
        break;
    }
    case MCI_STOP:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            MciSendCommandA_HookManager.unhook();
            mciSendCommandA(s_device_id, uMsg, fdwCommand, dwParam);
            mciSendCommandA(s_device_id, MCI_CLOSE, 0, NULL);
            MciSendCommandA_HookManager.hook();
            return 0;
        }
    }

    case MCI_CLOSE:
    {
        if (IDDevice == MAGIC_DEVICE_ID) {
            return 0;
        }
    }
    default:
        break;
    }

    MciSendCommandA_HookManager.unhook();
    auto ret = mciSendCommandA(IDDevice, uMsg, fdwCommand, dwParam);
    MciSendCommandA_HookManager.hook();
    return ret;
}

//*****************************************************
// ScriptStringAnalyse
//*****************************************************

extern "C" {
    _Check_return_ HRESULT WINAPI MyScriptStringAnalyse(
        HDC                    hdc,
        const void             * pString,
        int                    cString,
        int                    cGlyphs,
        int                    iCharset,
        DWORD                  dwFlags,
        int                    iReqWidth,
        SCRIPT_CONTROL         * psControl,
        SCRIPT_STATE           * psState,
        const int              * piDx,
        SCRIPT_TABDEF          * pTabdef,
        const BYTE             * pbInClass,
        SCRIPT_STRING_ANALYSIS * pssa);
}

static HookManager ScriptStringAnalyse_HookManager {
    getLibraryProcAddress(L"usp10.dll", "ScriptStringAnalyse"),
    MyScriptStringAnalyse
};

_Check_return_ HRESULT WINAPI MyScriptStringAnalyse(
    HDC                    hdc,
    const void             * pString,
    int                    cString,
    int                    cGlyphs,
    int                    iCharset,
    DWORD                  dwFlags,
    int                    iReqWidth,
    SCRIPT_CONTROL         * psControl,
    SCRIPT_STATE           * psState,
    const int              * piDx,
    SCRIPT_TABDEF          * pTabdef,
    const BYTE             * pbInClass,
    SCRIPT_STRING_ANALYSIS * pssa)
{
    if (cString <= 0 && iCharset != -1) // iCharset: -1 means input string is unicode
    {
        ScriptStringAnalyse_HookManager.unhook();
        auto ret = ScriptStringAnalyse(hdc, pString, cString, cGlyphs, iCharset, dwFlags, iReqWidth
            , psControl, psState, piDx, pTabdef, pbInClass, pssa);
        ScriptStringAnalyse_HookManager.hook();
        return ret;
    }

    int c = cString;
    const WCHAR* chs_string = (WCHAR*)pString;
    std::wstring cht_string = UTF16LE_CHS_To_CHT(chs_string, c);
    // 一對多的字由 printf 層用整句上下文決定, 此處保留原字不亂轉
    UTF16LE_KeepAmbiguousAsIs(cht_string, chs_string, c);

    ScriptStringAnalyse_HookManager.unhook();
    auto ret = ScriptStringAnalyse(hdc, cht_string.c_str(), cString, cGlyphs, iCharset, dwFlags, iReqWidth
        , psControl, psState, piDx, pTabdef, pbInClass, pssa);
    ScriptStringAnalyse_HookManager.hook();
    return ret;
}

//*****************************************************
// 特徵碼掃描功能 (AOB Scan)
//*****************************************************

static bool SigMatch(const BYTE* p, const unsigned char* sig, const char* mask)
{
    for (; *mask; ++mask, ++p, ++sig)
        if (*mask == 'x' && *p != *sig) return false;
    return true;
}

static PVOID FindAddress(const unsigned char* sig, const char* mask, DWORD hardcodedRva)
{
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (base == nullptr) return nullptr;

    auto dos = (PIMAGE_DOS_HEADER)base;
    auto nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    size_t sigLen = strlen(mask);

    // 1) 先驗證寫死的 RVA
    if (hardcodedRva + sigLen <= imageSize)
    {
        BYTE* guess = base + hardcodedRva;
        if (SigMatch(guess, sig, mask))
            return guess;
    }

    // 2) 掃描所有可執行區段
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s)
    {
        if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        BYTE* start = base + sec[s].VirtualAddress;
        DWORD size = sec[s].Misc.VirtualSize;
        if (size < sigLen) continue;
        for (BYTE* p = start; p <= start + size - sigLen; ++p)
            if (SigMatch(p, sig, mask))
                return p;
    }
    return nullptr;
}

// --- PrintfImpl ---
//   [功能] 核心 printf 實作，負責格式化所有遊戲內部的變長參數字串。
//   [特徵]
//     55 8B EC 83 EC 64           : push ebp; mov ebp,esp; sub esp,64h
//     C7 45 FC ?? ?? ?? ??        : mov [ebp-4], <abs_addr> (此位址隨版本變動)
//     8B 45 08 8B 08 89 4D BC     : eax=pArgs; ecx=*pArgs(fmt); [ebp-44h]=fmt
//     8B 55 08 83 C2 04 89 55 08  : pArgs+=4 (指向 va_list); [ebp+8]=pArgs
#define PRINTF_IMPL_RVA 0x0000E56B
static const unsigned char kPrintfSig[] = {
    0x55,0x8B,0xEC,0x83,0xEC,0x64,0xC7,0x45,0xFC, 0x00,0x00,0x00,0x00,
    0x8B,0x45,0x08,0x8B,0x08,0x89,0x4D,0xBC,
    0x8B,0x55,0x08,0x83,0xC2,0x04,0x89,0x55,0x08,
    0x8B,0x45,0x08,0x89,0x45,0xB8
};
static const char kPrintfMask[] = "xxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxx";

// --- GameTextPrintf ---
//   [功能] 遊戲繪圖引擎的文字輸出包裝函數，對話框逐字顯示時會頻繁呼叫。
//   [特徵]
//     55 8B EC                    : push ebp; mov ebp,esp
//     E8 ?? ?? ?? ??              : call <sync_state_func> (相對位址隨版本變動)
//     8D 45 0C 50                 : lea eax,[ebp+0Ch]; push eax (取得 fmt 的位址)
//     E8 ?? ?? ?? ??              : call <printf_impl> (相對位址隨版本變動)
#define GAME_TEXT_PRINTF_RVA 0x0000FAA0
static const unsigned char kGameTextPrintfSig[] = {
    0x55, 0x8B, 0xEC, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x8D, 0x45, 0x0C, 0x50, 0xE8, 0x00, 0x00, 0x00, 0x00
};
static const char kGameTextPrintfMask[] = "xxxx????xxxxx????";

// --- FullSentenceCall (DialogRender) ---
//   [功能] 對話框渲染進入點，此處傳入的是尚未拆解的完整簡體 GBK 句子。
//   [特徵]
//     55 8B EC 83 EC 18           : push ebp; mov ebp,esp; sub esp,18h
//     8B 45 10 89 45 F8           : eax=[ebp+10h](full_text); [ebp-8]=eax
//     C7 45 F4 00 00 00 00        : mov [ebp-0Ch], 0 (初始化計數器)
#define FULL_SENTENCE_CALL_RVA 0x0002C8B0
static const unsigned char kFullSentenceCallSig[] = {
    0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18, 0x8B, 0x45, 0x10, 0x89, 0x45, 0xF8, 0xC7, 0x45, 0xF4, 0x00, 0x00, 0x00, 0x00
};
static const char kFullSentenceCallMask[] = "xxxxxxxxxxxxxxxxxxx";

static PVOID g_PrintfImplAddr = nullptr;
static PVOID g_GameTextPrintfAddr = nullptr;
static PVOID g_FullSentenceCallAddr = nullptr;

static HookManager* g_PrintfImplHook = nullptr;
static HookManager* g_GameTextPrintfHook = nullptr;
static HookManager* g_FullSentenceCallHook = nullptr;

typedef int (__cdecl *PrintfImpl_t)(void** pArgs);
typedef int(__cdecl* GameTextPrintf_t)(void* arg1, const char* fmt, ...);
typedef int(__cdecl* FullSentenceCall_t)(void* arg1, void* arg2, const char* full_text, int arg4);

extern "C" {
    int __cdecl MyPrintfImpl(void** pArgs);
    int __cdecl MyGameTextPrintf(void* arg1, const char* fmt, ...);
    int __cdecl MyFullSentenceCall(void* arg1, void* arg2, const char* full_text, int arg4);
}

#ifdef _DEBUG
// DEBUG 組態才編入: 把轉換前/後文字與命中狀況寫到遊戲目錄下的 convert_log.txt
//   (字串為 GBK, 請以 GBK/ANSI 編碼開啟檢視)
#include <cstdio>
static void DebugLogConvert(const char* before, const char* after, bool matched, const ConvertStats& stats)
{
    FILE* fp = nullptr;
    if (fopen_s(&fp, LOG_FILE, "ab") != 0 || fp == nullptr) return;
    fputs("[before] ", fp); if (before != nullptr) fputs(before, fp); fputc('\n', fp);
    fputs("[after ] ", fp); if (after  != nullptr) fputs(after,  fp); fputc('\n', fp);
    fprintf(fp, "[match ] %s (phrase=%d, dict=%d)\n\n",
        matched ? "YES" : "no", stats.phraseHits, stats.dictHits);
    fclose(fp);
}
#endif

int __cdecl MyPrintfImpl(void** pArgs)
{
#ifdef _DEBUG
    PVOID callerAddr = _ReturnAddress();
    DWORD rva = (DWORD)((BYTE*)callerAddr - (BYTE*)GetModuleHandleW(nullptr));
    char* fmt = (pArgs != nullptr) ? (char*)pArgs[0] : nullptr;

    FILE* fp = nullptr;
    if (fopen_s(&fp, LOG_FILE, "ab") == 0 && fp != nullptr)
    {
        fprintf(fp, "[PrintfImpl @ RVA 0x%08X] fmt=%s\n", rva, fmt ? fmt : "null");
        fclose(fp);
    }
#endif

    g_PrintfImplHook->unhook();
    int ret = ((PrintfImpl_t)g_PrintfImplAddr)(pArgs);
    g_PrintfImplHook->hook();

    return ret;
}

int __cdecl MyFullSentenceCall(void* arg1, void* arg2, const char* full_text, int arg4)
{
    static char converted[8192];
    const char* target_text = full_text;

    if (full_text && strlen(full_text) > 0)
    {
#ifdef _DEBUG
        ConvertStats stats = { 0, 0 };
        bool matched = GBK_ResolveAmbiguousSentence(full_text, converted, sizeof(converted), &stats);
        if (matched) target_text = converted;
        DebugLogConvert(full_text, matched ? converted : full_text, matched, stats);
#else
        if (GBK_ResolveAmbiguousSentence(full_text, converted, sizeof(converted)))
        {
            target_text = converted;
        }
#endif
    }

    g_FullSentenceCallHook->unhook();
    int ret = ((FullSentenceCall_t)g_FullSentenceCallAddr)(arg1, arg2, target_text, arg4);
    g_FullSentenceCallHook->hook();

    return ret;
}

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

    FILE* fp = nullptr;
    if (fopen_s(&fp, LOG_FILE, "ab") == 0 && fp != nullptr)
    {
        fprintf(fp, "[GameTextPrintf @ RVA 0x%08X] %s\n", rva, buf);
        fclose(fp);
    }
#endif

    g_GameTextPrintfHook->unhook();
    // 使用 printf_impl 轉發以確保參數正確
    typedef int(__cdecl* printf_impl_t)(void**);
    int ret = ((printf_impl_t)g_PrintfImplAddr)((void**)&fmt);
    g_GameTextPrintfHook->hook();

    return ret;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        Read_Dictionary_File(L"dictionary.txt");
        Read_Phrase_File(L"phrases.txt");
        ScriptStringAnalyse_HookManager.hook();
        MciSendCommandA_HookManager.hook();

        // 1. Find and Hook PrintfImpl
        g_PrintfImplAddr = FindAddress(kPrintfSig, kPrintfMask, PRINTF_IMPL_RVA);
        if (g_PrintfImplAddr) {
            g_PrintfImplHook = new HookManager(g_PrintfImplAddr, MyPrintfImpl);
            g_PrintfImplHook->hook();
        }

        // 2. Find and Hook GameTextPrintf
        g_GameTextPrintfAddr = FindAddress(kGameTextPrintfSig, kGameTextPrintfMask, GAME_TEXT_PRINTF_RVA);
        if (g_GameTextPrintfAddr) {
            g_GameTextPrintfHook = new HookManager(g_GameTextPrintfAddr, MyGameTextPrintf);
            g_GameTextPrintfHook->hook();
        }

        // 3. Find and Hook FullSentenceCall
        g_FullSentenceCallAddr = FindAddress(kFullSentenceCallSig, kFullSentenceCallMask, FULL_SENTENCE_CALL_RVA);
        if (g_FullSentenceCallAddr) {
            g_FullSentenceCallHook = new HookManager(g_FullSentenceCallAddr, MyFullSentenceCall);
            g_FullSentenceCallHook->hook();
        }

#ifdef _DEBUG
        {
            FILE* fp = nullptr;
            if (fopen_s(&fp, LOG_FILE, "ab") == 0 && fp != nullptr)
            {
                BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
                auto logAddr = [&](const char* name, PVOID addr, DWORD rva) {
                    if (addr == nullptr)
                        fprintf(fp, "[%s] 特徵碼掃描失敗, 未 hook\n", name);
                    else
                        fprintf(fp, "[%s] addr=%p rva=0x%X %s\n",
                            name, addr, (unsigned)((BYTE*)addr - base),
                            ((BYTE*)addr == base + rva) ? "(寫死 RVA 命中)" : "(掃描找到)");
                };
                logAddr("printf_impl", g_PrintfImplAddr, PRINTF_IMPL_RVA);
                logAddr("GameTextPrintf", g_GameTextPrintfAddr, GAME_TEXT_PRINTF_RVA);
                logAddr("FullSentenceCall", g_FullSentenceCallAddr, FULL_SENTENCE_CALL_RVA);
                fputc('\n', fp);
                fclose(fp);
            }
        }
#endif
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

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
// printf_impl hook (整句簡轉繁: 一對多上下文修正)
//   遊戲所有文字都經過 Ekd5.exe 內的 printf_impl(void** pArgs),
//   pArgs[0] = 整句 GBK 格式字串。在這裡用整句上下文(詞庫)把一對多
//   的字改寫成正確繁體 GBK; 其餘單純轉換仍交給 ScriptStringAnalyse。
//   注意: PRINTF_IMPL_RVA 是此版 Ekd5.exe 的固定位址 (image base 0x400000),
//         換遊戲版本就要重新用呼叫堆疊找出新的位址。
//*****************************************************

// 已知此版 Ekd5.exe 的 printf_impl 位址 (image base 0x400000)。
#define PRINTF_IMPL_RVA 0x0000E56B

typedef int (__cdecl *PrintfImpl_t)(void** pArgs);

// printf_impl 進入點特徵碼 (signature / AOB)。中間的 imm32 是絕對位址(0x492e30),
// 會隨版本/重編而變, 以萬用字元 '?' 略過; 其餘是這個 printf_impl 獨有的序列。
//   55 8B EC 83 EC 64        push ebp; mov ebp,esp; sub esp,0x64
//   C7 45 FC ?? ?? ?? ??     mov [ebp-4], <abs>
//   8B 45 08 8B 08 89 4D BC  eax=pArgs; ecx=*pArgs(fmt); [ebp-0x44]=fmt
//   8B 55 08 83 C2 04 89 55 08   pArgs += 4 (va_list)
//   8B 45 08 89 45 B8        [ebp-0x48]=pArgs
static const unsigned char kPrintfSig[] = {
    0x55,0x8B,0xEC,0x83,0xEC,0x64,0xC7,0x45,0xFC, 0x00,0x00,0x00,0x00,
    0x8B,0x45,0x08,0x8B,0x08,0x89,0x4D,0xBC,
    0x8B,0x55,0x08,0x83,0xC2,0x04,0x89,0x55,0x08,
    0x8B,0x45,0x08,0x89,0x45,0xB8
};
static const char kPrintfMask[] = "xxxxxxxxx????xxxxxxxxxxxxxxxxxxxxxxx";

static bool SigMatch(const BYTE* p, const unsigned char* sig, const char* mask)
{
    for (; *mask; ++mask, ++p, ++sig)
        if (*mask == 'x' && *p != *sig) return false;
    return true;
}

// 定位 printf_impl: 先驗證寫死的 RVA, 不符再掃描所有可執行區段。找不到回 nullptr。
static PVOID FindPrintfImpl()
{
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    if (base == nullptr) return nullptr;

    auto dos = (PIMAGE_DOS_HEADER)base;
    auto nt  = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    size_t sigLen = sizeof(kPrintfSig);

    // 1) 先驗證寫死的 RVA 是否仍符合特徵 (快速路徑)
    if (PRINTF_IMPL_RVA + sigLen <= imageSize)
    {
        BYTE* guess = base + PRINTF_IMPL_RVA;
        if (SigMatch(guess, kPrintfSig, kPrintfMask))
            return guess;
    }

    // 2) RVA 不符 -> 掃描可執行區段找特徵 (換版本時自動找新位址)
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s)
    {
        if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        BYTE* start = base + sec[s].VirtualAddress;
        DWORD size  = sec[s].Misc.VirtualSize;
        if (size < sigLen) continue;
        for (BYTE* p = start; p <= start + size - sigLen; ++p)
            if (SigMatch(p, kPrintfSig, kPrintfMask))
                return p;
    }
    return nullptr;
}

static PVOID        g_PrintfImplAddr = nullptr;
static HookManager* g_PrintfImplHook = nullptr;

extern "C" {
    int __cdecl MyPrintfImpl(void** pArgs);
}

#ifdef _DEBUG
// DEBUG 組態才編入: 把轉換前/後文字與命中狀況寫到遊戲目錄下的 convert_log.txt
//   (字串為 GBK, 請以 GBK/ANSI 編碼開啟檢視)
#include <cstdio>
static void DebugLogConvert(const char* before, const char* after, bool matched, const ConvertStats& stats)
{
    FILE* fp = nullptr;
    if (fopen_s(&fp, "convert_log.txt", "ab") != 0 || fp == nullptr) return;
    fputs("[before] ", fp); if (before != nullptr) fputs(before, fp); fputc('\n', fp);
    fputs("[after ] ", fp); if (after  != nullptr) fputs(after,  fp); fputc('\n', fp);
    fprintf(fp, "[match ] %s (phrase=%d, dict=%d)\n\n",
        matched ? "YES" : "no", stats.phraseHits, stats.dictHits);
    fclose(fp);
}
#endif

int __cdecl MyPrintfImpl(void** pArgs)
{
    char* fmt = (pArgs != nullptr) ? (char*)pArgs[0] : nullptr;
    char* fmt_var = (pArgs != nullptr) ? (char*)pArgs[1] : nullptr; // 參考特徵碼註解 pArgs += 4 (va_list)
    char* saved_fmt = fmt;
    char* saved_var = fmt_var;
    static char converted[8192];

    char* target = fmt;
    bool isFmtS = (fmt != nullptr && strcmp(fmt, "%s") == 0);
    if (isFmtS) target = fmt_var;

#ifdef _DEBUG
    ConvertStats stats = { 0, 0 };
    bool matched = (target != nullptr) &&
        GBK_ResolveAmbiguousSentence(target, converted, sizeof(converted), &stats);
    if (matched) {
        if (isFmtS) pArgs[1] = converted;
        else pArgs[0] = converted;
    }
    if (target != nullptr) DebugLogConvert(target, matched ? converted : target, matched, stats);
#else
    if (target != nullptr && GBK_ResolveAmbiguousSentence(target, converted, sizeof(converted)))
    {
        if (isFmtS) pArgs[1] = converted;
        else pArgs[0] = converted;
    }
#endif

    g_PrintfImplHook->unhook();
    int ret = ((PrintfImpl_t)g_PrintfImplAddr)(pArgs);
    g_PrintfImplHook->hook();

    if (pArgs != nullptr) {
        pArgs[0] = saved_fmt;
        if (isFmtS) pArgs[1] = saved_var;
    }
    return ret;
}

//*****************************************************
// GameTextPrintf
//*****************************************************
#define GAME_TEXT_PRINTF_RVA 0x0000FAA0
#define FULL_SENTENCE_CALL_RVA 0x00031FCA
#define DIALOG_RENDER_RVA 0x0002C8B0

typedef int(__cdecl* GameTextPrintf_t)(void* arg1, const char* fmt, ...);

static PVOID GetGameTextPrintfAddr()
{
    return (PVOID)((BYTE*)GetModuleHandleW(nullptr) + GAME_TEXT_PRINTF_RVA);
}

static PVOID GetFullSentenceCallAddr()
{
    return (PVOID)((BYTE*)GetModuleHandleW(nullptr) + FULL_SENTENCE_CALL_RVA);
}

static PVOID GetDialogRenderAddr()
{
    return (PVOID)((BYTE*)GetModuleHandleW(nullptr) + DIALOG_RENDER_RVA);
}

extern "C" {
    int __cdecl MyGameTextPrintf(void* arg1, const char* fmt, ...);
    int __cdecl MyFullSentenceCallHook(void* arg1, const char* fmt, ...);
    int __cdecl MyDialogRenderHook(void* arg1, void* arg2, const char* full_text, int arg4);
}

static HookManager GameTextPrintf_HookManager{
    GetGameTextPrintfAddr(),
    MyGameTextPrintf
};

static HookManager FullSentenceCall_HookManager{
    GetFullSentenceCallAddr(),
    MyFullSentenceCallHook
};

static HookManager DialogRender_HookManager{
    GetDialogRenderAddr(),
    MyDialogRenderHook
};

int __cdecl MyDialogRenderHook(void* arg1, void* arg2, const char* full_text, int arg4)
{
    if (full_text && strlen(full_text) > 0)
    {
        FILE* fp = nullptr;
        if (fopen_s(&fp, "convert_log2.txt", "ab") == 0 && fp != nullptr)
        {
            fprintf(fp, "[DialogRender-Full] %s\n", full_text);
            fclose(fp);
        }
    }

    DialogRender_HookManager.unhook();
    // 原始函數是 55 8B EC 83 EC 18 (標準 prologue)
    // 我們直接呼叫原始位址
    typedef int(__cdecl* DialogRender_t)(void*, void*, const char*, int);
    int ret = ((DialogRender_t)GetDialogRenderAddr())(arg1, arg2, full_text, arg4);
    DialogRender_HookManager.hook();

    return ret;
}

int __cdecl MyFullSentenceCallHook(void* arg1, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsprintf_s(buf, fmt, args);
    va_end(args);

    FILE* fp = nullptr;
    if (fopen_s(&fp, "convert_log3.txt", "ab") == 0 && fp != nullptr)
    {
        fprintf(fp, "[FullSentenceCall@0x431FCA] %s\n", buf);
        fclose(fp);
    }

    FullSentenceCall_HookManager.unhook();
    // 這裡我們直接呼叫原始的 GameTextPrintf
    int ret = ((GameTextPrintf_t)GetGameTextPrintfAddr())(arg1, fmt, args); 
    // 同樣的，由於 0xFAA0 是包裝，我們也可以直接轉發給 printf_impl
    typedef int(__cdecl* printf_impl_t)(void**);
    printf_impl_t original_impl = (printf_impl_t)((BYTE*)GetModuleHandleW(nullptr) + 0xE56B);
    ret = original_impl((void**)&fmt);
    
    FullSentenceCall_HookManager.hook();

    return ret;
}

int __cdecl MyGameTextPrintf(void* arg1, const char* fmt, ...)
{
    PVOID callerAddr = _ReturnAddress();
    DWORD rva = (DWORD)((BYTE*)callerAddr - (BYTE*)GetModuleHandleW(nullptr));

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf_s(buf, fmt, args);
    va_end(args);

    FILE* fp = nullptr;
    if (fopen_s(&fp, "convert_log2.txt", "ab") == 0 && fp != nullptr)
    {
        fprintf(fp, "[GameTextPrintf @ RVA 0x%08X] %s\n", rva, buf);
        fclose(fp);
    }

    GameTextPrintf_HookManager.unhook();
    // 使用 printf_impl 轉發以確保參數正確
    typedef int(__cdecl* printf_impl_t)(void**);
    printf_impl_t original_impl = (printf_impl_t)((BYTE*)GetModuleHandleW(nullptr) + 0xE56B);
    int ret = original_impl((void**)&fmt);

    GameTextPrintf_HookManager.hook();

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
        GameTextPrintf_HookManager.hook();
        FullSentenceCall_HookManager.hook();
        DialogRender_HookManager.hook();

        // 用特徵碼定位 printf_impl (驗證寫死 RVA / 換版本時自動掃描)
        g_PrintfImplAddr = FindPrintfImpl();
        if (g_PrintfImplAddr != nullptr)
        {
            g_PrintfImplHook = new HookManager(g_PrintfImplAddr, MyPrintfImpl);
            g_PrintfImplHook->hook();
        }
#ifdef _DEBUG
        {
            FILE* fp = nullptr;
            if (fopen_s(&fp, "convert_log.txt", "ab") == 0 && fp != nullptr)
            {
                BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
                if (g_PrintfImplAddr == nullptr)
                    fprintf(fp, "[printf_impl] 特徵碼掃描失敗, 未 hook (請重新定位)\n\n");
                else
                    fprintf(fp, "[printf_impl] addr=%p rva=0x%X %s\n\n",
                        g_PrintfImplAddr,
                        (unsigned)((BYTE*)g_PrintfImplAddr - base),
                        ((BYTE*)g_PrintfImplAddr == base + PRINTF_IMPL_RVA)
                            ? "(寫死 RVA 命中)" : "(掃描找到, 與寫死 RVA 不同)");
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

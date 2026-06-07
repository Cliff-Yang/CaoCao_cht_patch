// dllmain.cpp : 定義 DLL 應用程式的進入點。
#include "pch.h"
#include <iostream>
#include "HookManager.hpp"
#include "ChsToCht.hpp"

#include "usp10.h"
#pragma comment(lib, "Usp10.lib")

#include <mciapi.h>
#include <digitalv.h>
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

#define PRINTF_IMPL_RVA 0x0000E56B

typedef int (__cdecl *PrintfImpl_t)(void** pArgs);

static PVOID GetPrintfImplAddr()
{
    return (PVOID)((BYTE*)GetModuleHandleW(nullptr) + PRINTF_IMPL_RVA);
}

extern "C" {
    int __cdecl MyPrintfImpl(void** pArgs);
}

static HookManager PrintfImpl_HookManager {
    GetPrintfImplAddr(),
    MyPrintfImpl
};

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
    char* saved = fmt;
    static char converted[8192];

#ifdef _DEBUG
    ConvertStats stats = { 0, 0 };
    bool matched = (fmt != nullptr) &&
        GBK_ResolveAmbiguousSentence(fmt, converted, sizeof(converted), &stats);
    if (matched) pArgs[0] = converted;
    if (fmt != nullptr) DebugLogConvert(fmt, matched ? converted : fmt, matched, stats);
#else
    if (fmt != nullptr && GBK_ResolveAmbiguousSentence(fmt, converted, sizeof(converted)))
        pArgs[0] = converted;
#endif

    PrintfImpl_HookManager.unhook();
    int ret = ((PrintfImpl_t)GetPrintfImplAddr())(pArgs);
    PrintfImpl_HookManager.hook();

    if (pArgs != nullptr) pArgs[0] = saved; // 還原, 避免動到呼叫者資料
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
        PrintfImpl_HookManager.hook();
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

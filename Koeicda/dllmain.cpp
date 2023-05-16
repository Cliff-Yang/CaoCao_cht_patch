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
    UTF16LE_FixOneToMany(cht_string);

    ScriptStringAnalyse_HookManager.unhook();
    auto ret = ScriptStringAnalyse(hdc, cht_string.c_str(), cString, cGlyphs, iCharset, dwFlags, iReqWidth
        , psControl, psState, piDx, pTabdef, pbInClass, pssa);
    ScriptStringAnalyse_HookManager.hook();
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
        std::wstring dictionary_file_path = L"dictionary.txt";
        Read_Dictionary_File(dictionary_file_path);
        ScriptStringAnalyse_HookManager.hook();
        MciSendCommandA_HookManager.hook();
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


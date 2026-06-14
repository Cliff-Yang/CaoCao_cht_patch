// dllmain.cpp : 定義 DLL 應用程式的進入點。
#include "pch.h"

// 各 API 的 hook (header-only, 每個檔 = 一個 API)。
// include 順序需維持 MciSendCommandA 在 ScriptStringAnalyse 之前,
// 以保留兩個 static HookManager 的建構順序 (見 CLAUDE.md)。
#include "api/MciSendCommandA.hpp"
#include "api/ScriptStringAnalyse.hpp"
#include "api/ReadFileHook.hpp"
#include "api/LoadMenuA.hpp"
#include "api/DialogTextHook.hpp"
#include "api/SetWindowTextA.hpp"
#include "api/SendMessageA.hpp"
#include "api/DrawTextExA.hpp"
#include "api/CreateFontA.hpp"

// 字典/詞庫載入 (簡轉繁共用)
#include "util/ChsToCht.hpp"

// 全域 hook 串行化鎖 (見 util/HookLock.hpp); 在此給出唯一定義。
CRITICAL_SECTION g_HookLock;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 導出函數 (DLL proxy: 轉發給改名後的原始 Koeicda_Origin.dll)
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

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        InitHookLock(); // 必須在任何 hook 安裝前初始化串行化鎖

        Read_Dictionary_File(L"dictionary.txt");
        Read_Phrase_File(L"phrases.txt");

        // 系統 API hook
        Install_ScriptStringAnalyse_Hook();
        Install_MciSendCommandA_Hook();
        Install_LoadMenuA_Hook();
        Install_DialogText_Hook();
        Install_SetWindowTextA_Hook();
        Install_SendMessageA_Hook();
        Install_DrawTextExA_Hook();
        Install_CreateFont_Hook();

        // 讀檔時簡轉繁: hook CreateFileA/ReadFile, 把 *.eex 劇本文字就地轉繁
        // (取代原本 hook 遊戲內部函數 FullSentenceCall/GameTextPrintf/PrintfImpl)
        Install_ReadFile_Hook();
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

#pragma once
#include "pch.h"
#include "../util/HookManager.hpp"
#include "../util/Address.hpp"
#include "../util/ChsToCht.hpp"
#include "../util/DebugLog.hpp"

//*****************************************************
// SetWindowTextA (user32.dll)
//   遊戲用 SetWindowTextA 設定視窗/對話框標題列 (GBK 簡體)。對話框標題在
//   WM_INITDIALOG 處理常式裡被重設, 會蓋掉我們在 DialogTextHook 的轉換;
//   主視窗 (非對話框) 也不會收到 WM_INITDIALOG。故在來源端攔截、把 GBK 標題
//   整串簡轉繁後再往下傳, 一併涵蓋兩者。
//*****************************************************

extern "C" BOOL WINAPI MySetWindowTextA(HWND hWnd, LPCSTR lpString);

static HookManager SetWindowTextA_HookManager {
    getLibraryProcAddress(L"user32.dll", "SetWindowTextA"),
    MySetWindowTextA
};

extern "C" BOOL WINAPI MySetWindowTextA(HWND hWnd, LPCSTR lpString)
{
    std::string converted;
    LPCSTR use = lpString;
    if (lpString != nullptr && GBK_FullConvert(lpString, converted))
    {
        use = converted.c_str();
#ifdef _DEBUG
        DebugLog("[SetWindowTextA] %s -> %s\n", lpString, converted.c_str());
#endif
    }

    SetWindowTextA_HookManager.unhook();
    BOOL ret = SetWindowTextA(hWnd, use);
    SetWindowTextA_HookManager.hook();
    return ret;
}

inline void Install_SetWindowTextA_Hook()
{
    SetWindowTextA_HookManager.hook();
}

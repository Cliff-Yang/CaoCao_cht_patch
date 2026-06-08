#pragma once
#include "pch.h"
#include "../util/ChsToCht.hpp"
#include "../util/DebugLog.hpp"

//*****************************************************
// 對話框文字簡轉繁 (WH_CALLWNDPROC, 攔 WM_INITDIALOG)
//   風云又起 等文字是 DIALOG 資源的控制項標題, 由 OS 對話框管理員繪製
//   (不經 Uniscribe, 也不經 LoadMenuA), 因此選單那條 hook 吃不到。
//   在對話框 WM_INITDIALOG 時 (控制項已建立、文字已從模板填入、尚未顯示),
//   列舉所有子控制項與對話框標題, 逐一 GetWindowTextW -> 轉 -> SetWindowTextW。
//   模態 (DialogBoxParamA) 與非模態 (CreateDialogParamA) 一視同仁。
//*****************************************************

#ifdef _DEBUG
static std::string DlgDbgW2U8(const std::wstring& ws)
{
    if (ws.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &s[0], n, nullptr, nullptr);
    return s;
}
#endif

// 把單一視窗 (對話框標題或控制項) 的文字簡轉繁, 有變更才寫回。
static void ConvertWindowText(HWND hWnd)
{
    int len = GetWindowTextLengthW(hWnd);
    if (len <= 0) return;

    std::wstring buf(len + 1, L'\0');
    int got = GetWindowTextW(hWnd, &buf[0], len + 1);
    buf.resize(got);
    if (buf.empty()) return;

    std::wstring cht = UTF16LE_FullConvert(buf);
    if (cht != buf)
    {
        SetWindowTextW(hWnd, cht.c_str());
#ifdef _DEBUG
        DebugLogU8("[DialogText] [before] %s\n", DlgDbgW2U8(buf).c_str());
        DebugLogU8("[DialogText] [after ] %s\n", DlgDbgW2U8(cht).c_str());
#endif
    }
}

static BOOL CALLBACK ConvertChildProc(HWND hChild, LPARAM /*lParam*/)
{
    ConvertWindowText(hChild);
    return TRUE; // 繼續列舉
}

static HHOOK g_dialogCallWndHook = NULL;

static LRESULT CALLBACK DialogCallWndProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        const CWPSTRUCT* p = (const CWPSTRUCT*)lParam;
        if (p->message == WM_INITDIALOG)
        {
            ConvertWindowText(p->hwnd);                        // 對話框標題列
            EnumChildWindows(p->hwnd, ConvertChildProc, 0);    // 所有控制項
        }
    }
    return CallNextHookEx(g_dialogCallWndHook, code, wParam, lParam);
}

inline void Install_DialogText_Hook()
{
    // 執行緒區域 hook: 掛在載入本 DLL 的執行緒 (即遊戲 UI 執行緒)。
    // 因 hook proc 就在本行程內, 不需要額外的全域 hook DLL。
    g_dialogCallWndHook = SetWindowsHookExW(
        WH_CALLWNDPROC, DialogCallWndProc, NULL, GetCurrentThreadId());
#ifdef _DEBUG
    DebugLogU8("[DialogText] Install hook=%p err=%lu\n",
        g_dialogCallWndHook, g_dialogCallWndHook ? 0 : GetLastError());
#endif
}

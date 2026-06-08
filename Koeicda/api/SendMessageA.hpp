#pragma once
#include "pch.h"
#include "../util/HookManager.hpp"
#include "../util/Address.hpp"
#include "../util/ChsToCht.hpp"
#include "../util/DebugLog.hpp"

//*****************************************************
// SendMessageA (user32.dll)
//   listbox / combobox 的「項目」不是 window text (GetWindowText 取不到),
//   而是遊戲透過 LB_ADDSTRING/LB_INSERTSTRING (combobox 為 CB_*) 以
//   SendMessageA 一條一條塞進去的; lParam 指向 GBK 簡體字串。這條路徑不經
//   ScriptStringAnalyse/LoadMenuA/DialogText/SetWindowText 任何一個 hook,
//   所以列表項目維持簡體。故在來源端攔這四個訊息, 把 lParam 字串整串簡轉繁
//   後再往下傳。其餘訊息一律早期放行 (這是熱路徑, 先用 Msg id 過濾)。
//
//   安全性: owner-draw 但「未設 *_HASSTRINGS」的 listbox/combobox, 其
//   LB_ADDSTRING 的 lParam 是任意 item data 而非字串指標, 誤當字串轉換會
//   crash。故只轉「真的存字串」的控制項 (非 owner-draw, 或有 HASSTRINGS),
//   並核對 class name 是 ListBox/ComboBox, 避免別的控制項剛好用到相同的
//   訊息數值。
//*****************************************************

extern "C" LRESULT WINAPI MySendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

static HookManager SendMessageA_HookManager {
    getLibraryProcAddress(L"user32.dll", "SendMessageA"),
    MySendMessageA
};

// 判斷這個 listbox/combobox 的 lParam 是否為字串 (而非 owner-draw item data)。
static bool SendMsg_LParamIsString(HWND hWnd, UINT Msg)
{
    wchar_t cls[16] = { 0 };
    int n = GetClassNameW(hWnd, cls, _countof(cls));
    if (n <= 0) return false;

    bool isList = (Msg == LB_ADDSTRING || Msg == LB_INSERTSTRING);
    // combobox 的下拉清單實作 class 為 "ComboLBox", 一併接受。
    bool classOk = isList
        ? (_wcsicmp(cls, L"ListBox") == 0)
        : (_wcsicmp(cls, L"ComboBox") == 0 || _wcsicmp(cls, L"ComboLBox") == 0);
    if (!classOk) return false;

    LONG style = GetWindowLongW(hWnd, GWL_STYLE);
    const LONG ownerDraw = LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE; // CBS_* 同值
    LONG hasStrings = isList ? LBS_HASSTRINGS : CBS_HASSTRINGS;
    return !(style & ownerDraw) || (style & hasStrings);
}

extern "C" LRESULT WINAPI MySendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    std::string converted;
    LPARAM use = lParam;
    if ((Msg == LB_ADDSTRING || Msg == LB_INSERTSTRING ||
         Msg == CB_ADDSTRING || Msg == CB_INSERTSTRING) &&
        lParam != 0 &&
        SendMsg_LParamIsString(hWnd, Msg) &&
        GBK_FullConvert((LPCSTR)lParam, converted))
    {
        use = (LPARAM)converted.c_str();
#ifdef _DEBUG
        DebugLog("[SendMessageA] [before] %s\n", (LPCSTR)lParam);
        DebugLog("[SendMessageA] [after ] %s\n", converted.c_str());
#endif
    }

    SendMessageA_HookManager.unhook();
    LRESULT ret = SendMessageA(hWnd, Msg, wParam, use);
    SendMessageA_HookManager.hook();
    return ret;
}

inline void Install_SendMessageA_Hook()
{
    SendMessageA_HookManager.hook();
}

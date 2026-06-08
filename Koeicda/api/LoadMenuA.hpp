#pragma once
#include "pch.h"
#include "../util/HookManager.hpp"
#include "../util/Address.hpp"
#include "../util/ChsToCht.hpp"
#include "../util/DebugLog.hpp"

//*****************************************************
// LoadMenuA (user32.dll)
//   遊戲用 LoadMenuA 載入 APPMENU 選單資源, 選單列由 OS 繪製 (不經 Uniscribe,
//   所以 ScriptStringAnalyse hook 吃不到)。攔截後在 SetMenu/DrawMenuBar 之前,
//   把每個選單項文字簡轉繁 (UTF16LE_FullConvert), 之後 OS 畫出來即為繁體。
//*****************************************************

extern "C" HMENU WINAPI MyLoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName);

static HookManager LoadMenuA_HookManager {
    getLibraryProcAddress(L"user32.dll", "LoadMenuA"),
    MyLoadMenuA
};

#ifdef _DEBUG
// 把 UTF-16 字串轉成 UTF-8 以便寫入 log.txt 後可讀
static std::string DbgW2U8(const std::wstring& ws)
{
    if (ws.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &s[0], n, nullptr, nullptr);
    return s;
}
#endif

// 遞迴把 HMENU 內每個項目的文字簡轉繁 (含子選單)。
static void ConvertMenuRecursive(HMENU hMenu)
{
    if (hMenu == NULL) return;

    int count = GetMenuItemCount(hMenu);
#ifdef _DEBUG
    DebugLog("[LoadMenuA] ConvertMenuRecursive hMenu=%p count=%d\n", hMenu, count);
#endif
    for (int i = 0; i < count; i++)
    {
        // 先以 dwTypeData = NULL 取得字串長度, 並順便拿子選單 handle
        MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
        mii.fMask = MIIM_STRING | MIIM_SUBMENU;
        mii.dwTypeData = NULL;
        if (!GetMenuItemInfoW(hMenu, i, TRUE, &mii))
            continue;

        HMENU hSub = mii.hSubMenu;

        if (mii.cch > 0) // cch == 0: 分隔線或無文字項, 略過
        {
            std::wstring buf(mii.cch + 1, L'\0');
            mii.dwTypeData = &buf[0];
            mii.cch = (UINT)buf.size();
            if (GetMenuItemInfoW(hMenu, i, TRUE, &mii))
            {
                buf.resize(mii.cch); // 截到實際長度 (不含結尾 0)

                std::wstring cht = UTF16LE_FullConvert(buf);
                bool changed = (cht != buf);
                BOOL setOk = FALSE;
                if (changed)
                {
                    MENUITEMINFOW set = { sizeof(MENUITEMINFOW) };
                    set.fMask = MIIM_STRING;
                    set.dwTypeData = const_cast<LPWSTR>(cht.c_str());
                    setOk = SetMenuItemInfoW(hMenu, i, TRUE, &set);
                }
#ifdef _DEBUG
                DebugLog("[LoadMenuA] item %d [before] %s\n", i, DbgW2U8(buf).c_str());
                DebugLog("[LoadMenuA] item %d [after ] %s (changed=%d setOk=%d err=%lu)\n",
                    i, DbgW2U8(cht).c_str(), changed ? 1 : 0, setOk ? 1 : 0,
                    changed ? GetLastError() : 0);
#endif
            }
        }

        if (hSub != NULL)
            ConvertMenuRecursive(hSub);
    }
}

extern "C" HMENU WINAPI MyLoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName)
{
    LoadMenuA_HookManager.unhook();
    HMENU hMenu = LoadMenuA(hInstance, lpMenuName);
    LoadMenuA_HookManager.hook();

#ifdef _DEBUG
    // lpMenuName 可能是字串或 MAKEINTRESOURCE 整數
    if (IS_INTRESOURCE(lpMenuName))
        DebugLog("[LoadMenuA] MyLoadMenuA called, name=#%u hMenu=%p\n",
            (unsigned)(ULONG_PTR)lpMenuName, hMenu);
    else
        DebugLog("[LoadMenuA] MyLoadMenuA called, name=\"%s\" hMenu=%p\n",
            lpMenuName ? lpMenuName : "(null)", hMenu);
#endif

    ConvertMenuRecursive(hMenu);
    return hMenu;
}

inline void Install_LoadMenuA_Hook()
{
#ifdef _DEBUG
    DebugLog("[LoadMenuA] Install_LoadMenuA_Hook\n");
#endif
    LoadMenuA_HookManager.hook();
}

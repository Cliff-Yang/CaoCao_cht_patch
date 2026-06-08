#pragma once
#include "pch.h"
#include "usp10.h"
#include "../util/HookManager.hpp"
#include "../util/Address.hpp"
#include "../util/ChsToCht.hpp"
#pragma comment(lib, "Usp10.lib")

//*****************************************************
// ScriptStringAnalyse (usp10.dll)
//   遊戲繪製字串的 Uniscribe text-shaping 呼叫。攔截輸入字串先做簡轉繁再往下傳,
//   是翻譯補丁的核心。一對多的字保留原字, 交給 printf 層用整句上下文決定。
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

inline void Install_ScriptStringAnalyse_Hook()
{
    ScriptStringAnalyse_HookManager.hook();
}

#pragma once
#include "pch.h"
#include "../util/HookManager.hpp"
#include "../util/Address.hpp"
#include "../util/DebugLog.hpp"

//*****************************************************
// CreateFontA/W / CreateFontIndirectA/W (gdi32.dll)
//   遊戲可能以 ANSI 或 Unicode 建立繪製用字體。這裡將所有建立的字體
//   一律導向標楷體 (DFKai-SB) 並開啟灰階抗鋸齒。
//*****************************************************

// 覆寫用字體名稱 (嘗試中文名提高相容性)
static const wchar_t* const OVERRIDE_FONT_FACE = L"Microsoft YaHei";

//=====================================================================
// 字體規則表 (逐項啟用/調整)
//---------------------------------------------------------------------
// 根據 log 觀察到的每一種 CreateFontA/W 請求各列一條規則，方便一條一條
// 開關與微調。比對方式：由上而下找第一條「enabled 且 src/charset/reqH
// 皆符合」的規則套用；找不到符合的 → 該字體維持遊戲原樣 (passthrough)。
//
// 各欄位意義：
//   enabled : 是否套用本條。false = 此種字體不動，照遊戲原樣繪製 (對照用)。
//   src     : 來源。SRC_A=CreateFontA / SRC_W=CreateFontW / SRC_ANY=兩者皆可。
//   charset : 比對遊戲請求的 charset (134=GB2312 中文, 1=DEFAULT, 2=SYMBOL)。
//             -1 = 不限。
//   reqH    : 比對遊戲請求的 lfHeight (含正負號)。0 = 不限。
//   useKai  : true = 換成標楷體 + DEFAULT_CHARSET + 灰階抗鋸齒；
//             false = 維持原 face / charset / quality (只可能改字高)。
//   outH    : 套用後實際使用的 lfHeight。
//               負值 = 字元高 (字身像素)，正值 = cell height。
//               填 0 = 沿用遊戲原本的 reqH 不變。
//   outW    : 套用後實際使用的 lfWidth。
//               填 0 = 自動 (依字高取自然寬高比，最不易變形)；
//               填 KEEP_W(=-1) = 沿用遊戲原本的 lfWidth。
//=====================================================================

enum FontSrc { SRC_ANY, SRC_A, SRC_W };

static const int KEEP_W = -1;   // outW 用：沿用遊戲原始 lfWidth

struct FontRule
{
    bool        enabled;
    FontSrc     src;
    int         charset;
    int         reqH;
    bool        useKai;
    int         outH;
    int         outW;
    const char* note;
};

// 觀察到的變體 (次數為 log 統計，僅供參考)。預設策略：中文字一律換標楷體、
// 正值字高取負以對齊字身、字寬改 0 取自然比例；符號字 (Marlett) 不動。
static FontRule g_FontRules[] = {
    // enabled  src     cs   reqH  useKai  outH  outW       note
    {  true,   SRC_A,  134,  -12,  true,   -12,  0,       "CreateFontA 主要內文 (x462)" },
    {  true,   SRC_A,  134,  -16,  true,   -16,  0,       "CreateFontA 較大字 (x1)" },
    {  true,   SRC_W,  134,   16,  true,   -16,  0,       "CreateFontW System 主要文字, 正值→取負 (x188)" },
    {  true,   SRC_W,  134,  -12,  true,   -12,  0,       "CreateFontW 宋体 內文 (x12)" },
    {  true,   SRC_W,  134,   32,  true,   -32,  0,       "CreateFontW 大標題 (x1)" },
    {  true,   SRC_W,    1,  -12,  true,   -12,  0,       "CreateFontW 對話框 宋体/JhengHei UI (x27)" },
    {  false,  SRC_W,    2,    0,  false,    0,  KEEP_W,  "CreateFontW Marlett 符號字 — 不動 (x5)" },
    // 其餘未列出的請求 → 不符合任何規則 → 維持遊戲原樣。
};
static const int g_FontRuleCount = (int)(sizeof(g_FontRules) / sizeof(g_FontRules[0]));

// 找出符合的規則；無則回傳 nullptr (維持原樣)。
inline const FontRule* MatchFontRule(FontSrc src, int charset, int reqH)
{
    for (int i = 0; i < g_FontRuleCount; ++i)
    {
        const FontRule& r = g_FontRules[i];
        if (!r.enabled) continue;
        if (r.src != SRC_ANY && r.src != src) continue;
        if (r.charset != -1 && r.charset != charset) continue;
        if (r.reqH != 0 && r.reqH != reqH) continue;
        return &r;
    }
    return nullptr;
}

// 依規則決定最終 lfHeight / lfWidth
inline int ResolveOutHeight(const FontRule* r, int reqH) { return r->outH != 0 ? r->outH : reqH; }
inline int ResolveOutWidth(const FontRule* r, int reqW) { return r->outW == KEEP_W ? reqW : r->outW; }

#ifdef _DEBUG
inline void LogResolvedFace(const char* tag, HFONT hf)
{
    if (hf == nullptr) { DebugLogU8("%s [resolved] (null HFONT)\n", tag); return; }
    HDC hdc = CreateCompatibleDC(nullptr);
    if (hdc == nullptr) return;
    HGDIOBJ old = SelectObject(hdc, hf);
    wchar_t face[LF_FACESIZE] = { 0 };
    GetTextFaceW(hdc, LF_FACESIZE, face);
    SelectObject(hdc, old);
    DeleteDC(hdc);
    char u8[256] = { 0 };
    WideCharToMultiByte(CP_UTF8, 0, face, -1, u8, sizeof(u8), nullptr, nullptr);
    DebugLogU8("%s [resolved] face=%s\n", tag, u8);
}
#endif

// Forward declarations
extern "C" {
    HFONT WINAPI MyCreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);
    HFONT WINAPI MyCreateFontW(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCWSTR);
    HFONT WINAPI MyCreateFontIndirectA(const LOGFONTA*);
    HFONT WINAPI MyCreateFontIndirectW(const LOGFONTW*);
}

// Hook Managers
static HookManager CreateFontA_HookManager { getLibraryProcAddress(L"gdi32.dll", "CreateFontA"), MyCreateFontA };
static HookManager CreateFontW_HookManager { getLibraryProcAddress(L"gdi32.dll", "CreateFontW"), MyCreateFontW };
static HookManager CreateFontIndirectA_HookManager { getLibraryProcAddress(L"gdi32.dll", "CreateFontIndirectA"), MyCreateFontIndirectA };
static HookManager CreateFontIndirectW_HookManager { getLibraryProcAddress(L"gdi32.dll", "CreateFontIndirectW"), MyCreateFontIndirectW };

extern "C" HFONT WINAPI MyCreateFontA(
    int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
    DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet,
    DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality,
    DWORD iPitchAndFamily, LPCSTR pszFaceName)
{
#ifdef _DEBUG
    DebugLog("[CreateFontA] h=%d w=%d cs=%lu face=%s\n",
        cHeight, cWidth, (unsigned long)iCharSet, pszFaceName ? pszFaceName : "(null)");
#endif
    const FontRule* r = MatchFontRule(SRC_A, (int)iCharSet, cHeight);

    // 無符合規則：維持遊戲原樣
    if (r == nullptr)
    {
        CreateFontA_HookManager.unhook();
        HFONT ret = CreateFontA(cHeight, cWidth, cEscapement, cOrientation, cWeight,
            bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality,
            iPitchAndFamily, pszFaceName);
        CreateFontA_HookManager.hook();
        return ret;
    }

    const int outH = ResolveOutHeight(r, cHeight);
    const int outW = ResolveOutWidth(r, cWidth);

    HFONT ret;
    if (r->useKai)
    {
        // 換標楷體：走寬版，DEFAULT_CHARSET + 灰階抗鋸齒
        CreateFontW_HookManager.unhook();
        ret = CreateFontW(cHeight, cWidth, cEscapement, cOrientation, cWeight,
            bItalic, bUnderline, bStrikeOut, DEFAULT_CHARSET,
            iOutPrecision, iClipPrecision, ANTIALIASED_QUALITY,
            iPitchAndFamily, OVERRIDE_FONT_FACE);
        CreateFontW_HookManager.hook();
    }
    else
    {
        // 不換 face，只調字高/字寬
        CreateFontA_HookManager.unhook();
        ret = CreateFontA(outH, outW, cEscapement, cOrientation, cWeight,
            bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality,
            iPitchAndFamily, pszFaceName);
        CreateFontA_HookManager.hook();
    }
#ifdef _DEBUG
    LogResolvedFace("[CreateFontA]", ret);
#endif
    return ret;
}

extern "C" HFONT WINAPI MyCreateFontW(
    int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
    DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet,
    DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality,
    DWORD iPitchAndFamily, LPCWSTR pszFaceName)
{
#ifdef _DEBUG
    char u8[LF_FACESIZE * 3] = { 0 };
    if (pszFaceName) WideCharToMultiByte(CP_UTF8, 0, pszFaceName, -1, u8, sizeof(u8), nullptr, nullptr);
    DebugLogU8("[CreateFontW] h=%d w=%d cs=%lu face=%s\n",
        cHeight, cWidth, (unsigned long)iCharSet, pszFaceName ? u8 : "(null)");
#endif
    const FontRule* r = MatchFontRule(SRC_W, (int)iCharSet, cHeight);

    // 無符合規則：維持遊戲原樣
    if (r == nullptr)
    {
        CreateFontW_HookManager.unhook();
        HFONT ret = CreateFontW(cHeight, cWidth, cEscapement, cOrientation, cWeight,
            bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality,
            iPitchAndFamily, pszFaceName);
        CreateFontW_HookManager.hook();
        return ret;
    }

    const int outH = ResolveOutHeight(r, cHeight);
    const int outW = ResolveOutWidth(r, cWidth);

    CreateFontW_HookManager.unhook();
    HFONT ret;
    if (r->useKai)
    {
        ret = CreateFontW(outH, outW, cEscapement, cOrientation, cWeight,
            bItalic, bUnderline, bStrikeOut, DEFAULT_CHARSET,
            iOutPrecision, iClipPrecision, ANTIALIASED_QUALITY,
            iPitchAndFamily, OVERRIDE_FONT_FACE);
    }
    else
    {
        ret = CreateFontW(outH, outW, cEscapement, cOrientation, cWeight,
            bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality,
            iPitchAndFamily, pszFaceName);
    }
    CreateFontW_HookManager.hook();
#ifdef _DEBUG
    LogResolvedFace("[CreateFontW]", ret);
#endif
    return ret;
}

extern "C" HFONT WINAPI MyCreateFontIndirectA(const LOGFONTA* lplf)
{
    if (lplf == nullptr) return nullptr;
    return MyCreateFontA(lplf->lfHeight, lplf->lfWidth, lplf->lfEscapement, lplf->lfOrientation,
        lplf->lfWeight, lplf->lfItalic, lplf->lfUnderline, lplf->lfStrikeOut, lplf->lfCharSet,
        lplf->lfOutPrecision, lplf->lfClipPrecision, lplf->lfQuality, lplf->lfPitchAndFamily, lplf->lfFaceName);
}

extern "C" HFONT WINAPI MyCreateFontIndirectW(const LOGFONTW* lplf)
{
    if (lplf == nullptr) return nullptr;
    return MyCreateFontW(lplf->lfHeight, lplf->lfWidth, lplf->lfEscapement, lplf->lfOrientation,
        lplf->lfWeight, lplf->lfItalic, lplf->lfUnderline, lplf->lfStrikeOut, lplf->lfCharSet,
        lplf->lfOutPrecision, lplf->lfClipPrecision, lplf->lfQuality, lplf->lfPitchAndFamily, lplf->lfFaceName);
}

inline void Install_CreateFont_Hook()
{
     //CreateFontA_HookManager.hook();
     //CreateFontW_HookManager.hook();
     //CreateFontIndirectA_HookManager.hook();
     //CreateFontIndirectW_HookManager.hook();
}

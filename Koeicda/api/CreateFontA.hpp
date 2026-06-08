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
static const wchar_t* const OVERRIDE_FONT_FACE = L"標楷體";

// 替換字體的字高縮放係數
static const double FONT_HEIGHT_SCALE = 1.0;

inline int ScaleFontHeight(int h)
{
    if (FONT_HEIGHT_SCALE == 1.0) return h;
    double v = h * FONT_HEIGHT_SCALE;
    return (int)(v >= 0 ? v + 0.5 : v - 0.5);
}

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

// 核心實作: 統一處理字體參數覆寫
inline HFONT InternalCreateFontW(
    int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
    DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet,
    DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality,
    DWORD iPitchAndFamily, LPCWSTR pszFaceName)
{
    CreateFontW_HookManager.unhook();
    // 使用 DEFAULT_CHARSET 以支援 Unicode 繪製並避免編碼衝突造成的亂碼
    HFONT ret = CreateFontW(
        ScaleFontHeight(cHeight), cWidth, cEscapement, cOrientation, cWeight,
        bItalic, bUnderline, bStrikeOut, DEFAULT_CHARSET,
        iOutPrecision, iClipPrecision, ANTIALIASED_QUALITY,
        iPitchAndFamily, OVERRIDE_FONT_FACE);
    CreateFontW_HookManager.hook();
    return ret;
}

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
    HFONT ret = InternalCreateFontW(cHeight, cWidth, cEscapement, cOrientation, cWeight,
        bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality,
        iPitchAndFamily, nullptr);
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
    HFONT ret = InternalCreateFontW(cHeight, cWidth, cEscapement, cOrientation, cWeight,
        bItalic, bUnderline, bStrikeOut, iCharSet, iOutPrecision, iClipPrecision, iQuality,
        iPitchAndFamily, pszFaceName);
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
    // CreateFontA_HookManager.hook();
    // CreateFontW_HookManager.hook();
    // CreateFontIndirectA_HookManager.hook();
    // CreateFontIndirectW_HookManager.hook();
}

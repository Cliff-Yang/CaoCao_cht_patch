#pragma once
#include "pch.h"
#include "../util/HookManager.hpp"
#include "../util/Address.hpp"
#include "../util/DebugLog.hpp"

//*****************************************************
// CreateFontA / CreateFontIndirectA (gdi32.dll)
//   遊戲以 GBK (簡體 ANSI) 建立繪製用字體, 預設字面為宋体且多為無抗鋸齒,
//   外觀不佳。這裡把所有建立的字體一律改成「楷体」(Windows 內建 simkai.ttf,
//   標楷體風格), 並開啟灰階抗鋸齒。
//
//   要點:
//   - 改呼叫寬版 CreateFontW / CreateFontIndirectW, 字面用寬字面 L"楷体",
//     避開 ANSI/GBK 編碼問題 (本檔以 UTF-8 BOM 儲存, 寬字面才會編成正確 UTF-16)。
//   - charset 一律沿用遊戲原值: 轉換後的字串仍以 GBK 繪製, 而 楷体(simkai)
//     支援 GB2312/GBK 且涵蓋繁體字形, 故無需更動 charset。
//   - quality 覆寫為 ANTIALIASED_QUALITY (灰階抗鋸齒); 刻意不用 CLEARTYPE,
//     以免在點陣/透明背景上出現次像素彩邊。
//*****************************************************

// 覆寫用字體名稱。
// [驗證階段] 暫用 Microsoft YaHei (Win10 必定內建, 無襯線, 與宋体一眼可辨;
// 純 ASCII 名稱可同時排除 CJK 字面編碼問題)。確認 hook 有效後再換回楷書字體。
static const wchar_t* const OVERRIDE_FONT_FACE = L"Microsoft YaHei";

#ifdef _DEBUG
// 診斷: 把 GDI 實際解析到的 face 名稱寫到 unicode log (UTF-8 編碼檢視)。
// 即使視覺上看不出差異, 也能據此判斷 GDI 是否真的給了我們指定的字體,
// 或因 charset/face 不一致而退回宋体。
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

extern "C" HFONT WINAPI MyCreateFontA(
    int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
    DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet,
    DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality,
    DWORD iPitchAndFamily, LPCSTR pszFaceName);

extern "C" HFONT WINAPI MyCreateFontIndirectA(const LOGFONTA* lplf);

static HookManager CreateFontA_HookManager {
    getLibraryProcAddress(L"gdi32.dll", "CreateFontA"),
    MyCreateFontA
};

static HookManager CreateFontIndirectA_HookManager {
    getLibraryProcAddress(L"gdi32.dll", "CreateFontIndirectA"),
    MyCreateFontIndirectA
};

extern "C" HFONT WINAPI MyCreateFontA(
    int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
    DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet,
    DWORD iOutPrecision, DWORD iClipPrecision, DWORD iQuality,
    DWORD iPitchAndFamily, LPCSTR pszFaceName)
{
#ifdef _DEBUG
    DebugLog("[CreateFontA] [before] face=%s charset=%lu quality=%lu\n",
        pszFaceName ? pszFaceName : "(null)",
        (unsigned long)iCharSet, (unsigned long)iQuality);
#endif

    CreateFontA_HookManager.unhook();
    // 改走寬版: 覆寫字面為楷体、開灰階抗鋸齒; 其餘 (含 charset) 沿用原值。
    HFONT ret = CreateFontW(
        cHeight, cWidth, cEscapement, cOrientation, cWeight,
        bItalic, bUnderline, bStrikeOut, iCharSet,
        iOutPrecision, iClipPrecision, ANTIALIASED_QUALITY,
        iPitchAndFamily, OVERRIDE_FONT_FACE);
    CreateFontA_HookManager.hook();
#ifdef _DEBUG
    LogResolvedFace("[CreateFontA]", ret);
#endif
    return ret;
}

extern "C" HFONT WINAPI MyCreateFontIndirectA(const LOGFONTA* lplf)
{
    if (lplf == nullptr)
    {
        // 理論上不會發生; 保險起見原樣轉呼叫真函式。
        CreateFontIndirectA_HookManager.unhook();
        HFONT ret = CreateFontIndirectA(lplf);
        CreateFontIndirectA_HookManager.hook();
        return ret;
    }

#ifdef _DEBUG
    DebugLog("[CreateFontIndirectA] [before] face=%s charset=%lu quality=%lu\n",
        lplf->lfFaceName,
        (unsigned long)lplf->lfCharSet, (unsigned long)lplf->lfQuality);
#endif

    // 把 LOGFONTA 數值欄位複製到 LOGFONTW, 再覆寫字面與 quality。
    LOGFONTW lfw{};
    lfw.lfHeight         = lplf->lfHeight;
    lfw.lfWidth          = lplf->lfWidth;
    lfw.lfEscapement     = lplf->lfEscapement;
    lfw.lfOrientation    = lplf->lfOrientation;
    lfw.lfWeight         = lplf->lfWeight;
    lfw.lfItalic         = lplf->lfItalic;
    lfw.lfUnderline      = lplf->lfUnderline;
    lfw.lfStrikeOut      = lplf->lfStrikeOut;
    lfw.lfCharSet        = lplf->lfCharSet;      // 沿用原 charset (字串仍以 GBK 繪製)
    lfw.lfOutPrecision   = lplf->lfOutPrecision;
    lfw.lfClipPrecision  = lplf->lfClipPrecision;
    lfw.lfQuality        = ANTIALIASED_QUALITY;  // 開灰階抗鋸齒
    lfw.lfPitchAndFamily = lplf->lfPitchAndFamily;
    wcscpy_s(lfw.lfFaceName, LF_FACESIZE, OVERRIDE_FONT_FACE);

    CreateFontIndirectA_HookManager.unhook();
    HFONT ret = CreateFontIndirectW(&lfw);
    CreateFontIndirectA_HookManager.hook();
#ifdef _DEBUG
    LogResolvedFace("[CreateFontIndirectA]", ret);
#endif
    return ret;
}

inline void Install_CreateFont_Hook()
{
    // CreateFontA_HookManager.hook();
    // CreateFontIndirectA_HookManager.hook();
}

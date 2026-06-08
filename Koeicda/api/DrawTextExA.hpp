#pragma once
#include "pch.h"
#include "../util/HookManager.hpp"
#include "../util/Address.hpp"
#include "../util/ChsToCht.hpp"
#include "../util/DebugLog.hpp"

//*****************************************************
// DrawTextExA (user32.dll)
//   「讀取進度／保存進度」的存檔欄位列表是用 DrawTextExA 直接繪製的 (存檔描述
//   為動態文字、來源是存檔檔, 不在 exe 字串內; 這條路徑不經我們的
//   ScriptStringAnalyse hook, 所以維持簡體)。故在來源端攔截, 把 GBK 文字整串
//   簡轉繁 (GBK_FullConvert, 與 SetWindowTextA/SendMessageA 共用) 後再往下傳。
//
//   轉換結果寫進獨立可寫緩衝, 不動原 buffer:
//     - DrawTextExA 在 DT_MODIFYSTRING 下會回寫字串 (省略號等), 故緩衝預留 slack;
//     - 也避免改到呼叫端傳入的唯讀字串字面 (in-place 改會 AV)。
//   GBK 簡繁多為等長 2-byte, 故 byte 長度通常不變; 一律以轉換後字串長度往下傳。
//*****************************************************

extern "C" int WINAPI MyDrawTextExA(HDC hdc, LPSTR lpchText, int cchText,
                                    LPRECT lprc, UINT format, LPDRAWTEXTPARAMS lpdtp);

static HookManager DrawTextExA_HookManager {
    getLibraryProcAddress(L"user32.dll", "DrawTextExA"),
    MyDrawTextExA
};

extern "C" int WINAPI MyDrawTextExA(HDC hdc, LPSTR lpchText, int cchText,
                                    LPRECT lprc, UINT format, LPDRAWTEXTPARAMS lpdtp)
{
    std::vector<char> buf;
    LPSTR use = lpchText;
    int   useLen = cchText;

    if (lpchText != nullptr)
    {
        std::string src = (cchText < 0) ? std::string(lpchText)
                                        : std::string(lpchText, cchText);
        std::string converted;
        if (GBK_FullConvert(src.c_str(), converted))
        {
            buf.assign(converted.begin(), converted.end());
            buf.resize(converted.size() + 8, '\0'); // null + DT_MODIFYSTRING 回寫 slack
            use = buf.data();
            useLen = (cchText < 0) ? -1 : (int)converted.size();
#ifdef _DEBUG
            DebugLog("[DrawTextExA] [before] %s\n", src.c_str());
            DebugLog("[DrawTextExA] [after ] %s\n", converted.c_str());
#endif
        }
    }

    DrawTextExA_HookManager.unhook();
    int ret = DrawTextExA(hdc, use, useLen, lprc, format, lpdtp);
    DrawTextExA_HookManager.hook();
    return ret;
}

inline void Install_DrawTextExA_Hook()
{
    DrawTextExA_HookManager.hook();
}

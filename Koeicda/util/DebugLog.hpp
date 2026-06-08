#pragma once

// 共用的 DEBUG 記錄工具: 僅在 _DEBUG 組態下編入。分成兩個記錄檔, 因兩類文字
// 編碼不同, 混在同一檔會無法用單一編碼正確檢視:
//   - DebugLog   -> log_gbk.txt     : 遊戲 GBK (簡體 ANSI) 原文, 請以 GBK/ANSI 檢視
//   - DebugLogU8 -> log_unicode.txt : dialog/選單等資源文字 (已轉 UTF-8), 請以 UTF-8 檢視
// 兩者皆以 append 模式寫到遊戲目錄下。Release 組態下整段不存在, 不影響行為與效能。
#ifdef _DEBUG
#include <cstdio>
#include <cstdarg>

#define LOG_FILE_GBK     "log_gbk.txt"
#define LOG_FILE_UNICODE "log_unicode.txt"

// 共用實作: 以 append 模式把格式化結果寫到指定檔案。
inline void DebugLogTo(const char* path, const char* fmt, va_list args)
{
    FILE* fp = nullptr;
    if (fopen_s(&fp, path, "ab") != 0 || fp == nullptr) return;
    vfprintf(fp, fmt, args);
    fclose(fp);
}

// 遊戲 GBK 原文 -> log_gbk.txt (請以 GBK/ANSI 檢視)
inline void DebugLog(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DebugLogTo(LOG_FILE_GBK, fmt, args);
    va_end(args);
}

// dialog/選單等資源的 UTF-8 文字 -> log_unicode.txt (請以 UTF-8 檢視)
inline void DebugLogU8(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    DebugLogTo(LOG_FILE_UNICODE, fmt, args);
    va_end(args);
}
#endif

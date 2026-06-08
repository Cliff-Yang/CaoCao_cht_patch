#pragma once

// 共用的 DEBUG 記錄工具: 僅在 _DEBUG 組態下編入, 把格式化字串以 append 模式
// 寫到遊戲目錄下的 log.txt。Release 組態下整段不存在, 不影響行為與效能。
#ifdef _DEBUG
#include <cstdio>
#include <cstdarg>

#define LOG_FILE "log.txt"

inline void DebugLog(const char* fmt, ...)
{
    FILE* fp = nullptr;
    if (fopen_s(&fp, LOG_FILE, "ab") != 0 || fp == nullptr) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fclose(fp);
}
#endif

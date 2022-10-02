#pragma once
#include "pch.h"

//用於將地址轉換為byte數組的類，其實用union也可以辦到，
//不過是C++的未定義行為，所以這裡寫了一個轉換類
class Address
{
private:
    enum { SIZE = 4 };
    BYTE bytes[SIZE];
public:
    const BYTE operator[](int i) const
    {
        return bytes[i];
    }
    Address(LPVOID address)
    {
        memcpy(bytes, &address, SIZE);
    }
    Address(DWORD address)
    {
        memcpy(bytes, &address, SIZE);
    }
};

// 獲取dll中指定函數的地址
FARPROC getLibraryProcAddress(LPCWSTR libName, LPCSTR procName)
{
    HMODULE hModule = GetModuleHandle(libName);
    if (!hModule) throw std::runtime_error("Unable to load library!");

    FARPROC procAddress = GetProcAddress(hModule, procName);
    if (!procAddress) throw std::runtime_error("Unable to get proc address!");
    return procAddress;
}
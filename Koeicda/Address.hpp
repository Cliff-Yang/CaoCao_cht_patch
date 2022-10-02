#pragma once
#include "pch.h"

//�Ω�N�a�}�ഫ��byte�Ʋժ����A����union�]�i�H���A
//���L�OC++�����w�q�欰�A�ҥH�o�̼g�F�@���ഫ��
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

// ���dll�����w��ƪ��a�}
FARPROC getLibraryProcAddress(LPCWSTR libName, LPCSTR procName)
{
    HMODULE hModule = GetModuleHandle(libName);
    if (!hModule) throw std::runtime_error("Unable to load library!");

    FARPROC procAddress = GetProcAddress(hModule, procName);
    if (!procAddress) throw std::runtime_error("Unable to get proc address!");
    return procAddress;
}
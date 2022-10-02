#pragma once
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <locale>
#include <codecvt>

static std::map<wchar_t, wchar_t> UTF16_CHS_TO_CHT_Dictionary;

void Read_Dictionary_File(std::wstring dictionary_file_path)
{
    std::ifstream inFile(dictionary_file_path);
    std::string chs_str, tmp_str, cht_str;
    if (inFile.good())
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        bool isBomHandled = false;
        while (inFile >> chs_str >> tmp_str >> cht_str)
        {
            if (!isBomHandled && chs_str.length() > 3) // Handle UTF-8 BOM
            {
                isBomHandled = true;
                chs_str = chs_str.substr(chs_str.length() - 3, 3);
            }

            wchar_t utf16_chs = converter.from_bytes(chs_str)[0];
            wchar_t utf16_cht = converter.from_bytes(cht_str)[0];
            UTF16_CHS_TO_CHT_Dictionary.insert(std::pair<wchar_t, wchar_t>(utf16_chs, utf16_cht));
        }
        inFile.close();
    }
}

// UTF-16LE的簡體字 => UTF-16LE的繁體字
std::wstring UTF16LE_CHS_To_CHT(LPCWSTR chs_string, UINT word_len)
{
    WORD wLangID = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
    LCID locale = MAKELCID(wLangID, SORT_CHINESE_PRC);

    WCHAR* cht_string = new WCHAR[word_len + 1];
    cht_string[word_len] = 0;
    LCMapStringW(locale, LCMAP_TRADITIONAL_CHINESE, chs_string, word_len, cht_string, word_len);

    std::wstring ret_string(cht_string);
    delete[] cht_string;
    cht_string = NULL;
    return ret_string;
}

// UTF-16LE 簡轉繁: 一對多修正
void UTF16LE_FixOneToMany(std::wstring& wsInput)
{
    for (size_t i = 0; i < wsInput.size(); i++)
    {
        wchar_t wc = wsInput[i];
        if (UTF16_CHS_TO_CHT_Dictionary.count(wc) > 0)
        {
            wsInput[i] = UTF16_CHS_TO_CHT_Dictionary[wc];
        }
    }
}

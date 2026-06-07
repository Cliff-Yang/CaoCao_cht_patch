#pragma once
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <iostream>
#include <locale>
#include <codecvt>
#include <cstring>

static std::map<wchar_t, wchar_t> UTF16_CHS_TO_CHT_Dictionary;

// 詞庫: 簡體詞 -> 繁體詞 (一對多的上下文例外, 整句階段貪婪最長匹配)
static std::vector<std::pair<std::wstring, std::wstring>> CHS_TO_CHT_Phrases;
static size_t MaxPhraseLen = 0;

// 轉換統計 (DEBUG log 用): 詞庫命中數 / 單字 dictionary 命中數
struct ConvertStats
{
    int phraseHits;
    int dictHits;
};

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

// draw 端 (ScriptStringAnalyse) 用: 一對多的字交給 printf 層用整句上下文決定,
// 這裡把它們還原成輸入的原字, 不要在逐字階段亂轉。
void UTF16LE_KeepAmbiguousAsIs(std::wstring& out, const WCHAR* in, UINT len)
{
    for (size_t i = 0; i < out.size() && i < len; i++)
    {
        if (UTF16_CHS_TO_CHT_Dictionary.count(in[i]) > 0)
            out[i] = in[i];
    }
}

// 讀詞庫檔: 每行 "簡詞 : 繁詞" (UTF-8)。簡繁字數需相同才收錄(維持索引對齊)。
void Read_Phrase_File(std::wstring phrase_file_path)
{
    std::ifstream inFile(phrase_file_path);
    std::string chs_str, tmp_str, cht_str;
    if (inFile.good())
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        bool isBomHandled = false;
        while (inFile >> chs_str >> tmp_str >> cht_str)
        {
            if (!isBomHandled) // 去掉第一個 token 開頭的 UTF-8 BOM
            {
                isBomHandled = true;
                if (chs_str.size() >= 3 &&
                    (unsigned char)chs_str[0] == 0xEF &&
                    (unsigned char)chs_str[1] == 0xBB &&
                    (unsigned char)chs_str[2] == 0xBF)
                    chs_str = chs_str.substr(3);
            }

            std::wstring chs = converter.from_bytes(chs_str);
            std::wstring cht = converter.from_bytes(cht_str);
            if (chs.empty() || chs.length() != cht.length())
                continue;

            CHS_TO_CHT_Phrases.emplace_back(chs, cht);
            if (chs.length() > MaxPhraseLen) MaxPhraseLen = chs.length();
        }
        inFile.close();
    }
}

// 整句 GBK(CP936) 的一對多上下文修正:
//   先用詞庫貪婪最長匹配, 未被詞庫覆蓋的一對多字再套單字 dictionary。
//   其餘字(含非中文/格式字元)保持原樣, 交給 ScriptStringAnalyse 處理。
//   有變更才回傳 true 並把結果寫入 out(GBK)。
bool GBK_ResolveAmbiguousSentence(const char* gbk, char* out, size_t outsz, ConvertStats* stats = nullptr)
{
    if (stats != nullptr) { stats->phraseHits = 0; stats->dictHits = 0; }
    if (gbk == nullptr) return false;

    int wlen = MultiByteToWideChar(936, 0, gbk, -1, nullptr, 0); // 含結尾 0
    if (wlen <= 1) return false;
    std::wstring ws(wlen, L'\0');
    MultiByteToWideChar(936, 0, gbk, -1, &ws[0], wlen);
    if (!ws.empty() && ws.back() == L'\0') ws.pop_back();

    std::wstring orig = ws;
    bool changed = false;

    // 1) 詞庫貪婪最長匹配
    for (size_t i = 0; i < ws.size(); )
    {
        bool matched = false;
        size_t maxL = ws.size() - i;
        if (maxL > MaxPhraseLen) maxL = MaxPhraseLen;
        for (size_t L = maxL; L >= 1 && !matched; L--)
        {
            for (const auto& p : CHS_TO_CHT_Phrases)
            {
                if (p.first.length() == L && ws.compare(i, L, p.first) == 0)
                {
                    ws.replace(i, L, p.second);
                    changed = true;
                    if (stats != nullptr) stats->phraseHits++;
                    i += L;
                    matched = true;
                    break;
                }
            }
        }
        if (!matched)
        {
            // 2) 未被詞庫覆蓋的一對多字 -> 套單字預設對應
            auto it = UTF16_CHS_TO_CHT_Dictionary.find(ws[i]);
            if (it != UTF16_CHS_TO_CHT_Dictionary.end() && ws[i] != it->second)
            {
                ws[i] = it->second;
                changed = true;
                if (stats != nullptr) stats->dictHits++;
            }
            i++;
        }
    }

    if (!changed) return false;

    // 3) UTF16 -> GBK, 逐字編碼; 編不回 GBK 的字退回原字
    std::string result;
    for (size_t i = 0; i < ws.size(); i++)
    {
        wchar_t wc = ws[i];
        char mb[8];
        BOOL usedDefault = FALSE;
        int n = WideCharToMultiByte(936, 0, &wc, 1, mb, sizeof(mb), nullptr, &usedDefault);
        if (n > 0 && !usedDefault)
        {
            result.append(mb, n);
        }
        else if (i < orig.size())
        {
            wchar_t oc = orig[i];
            int n2 = WideCharToMultiByte(936, 0, &oc, 1, mb, sizeof(mb), nullptr, nullptr);
            if (n2 > 0) result.append(mb, n2);
        }
    }

    if (result.size() + 1 > outsz) return false; // 太長, 安全起見放棄
    memcpy(out, result.c_str(), result.size() + 1);
    return true;
}

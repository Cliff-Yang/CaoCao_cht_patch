#pragma once
#include "pch.h"
#include <string>
#include "ChsToCht.hpp"
#ifdef _DEBUG
#include "DebugLog.hpp"
#endif

//*****************************************************
// EEX 劇本檔解析 (自 CaoCaoEditor 的 EEX.cs 精簡移植)
//
//   R_*.eex / S_*.eex 是 "EEX\0" magic 開頭的 bytecode 劇本檔, 對白與 UI 文字
//   以純 GBK 簡體字串內嵌在指令流中 (無加密/壓縮)。本檔在「讀檔當下」走訪指令
//   結構, 逢文字指令就把該段 GBK 文字就地簡轉繁 (GBK_FullConvert), 因 GBK 簡→
//   GBK 繁 每字皆 2 bytes 等長, 直接覆寫即可 —— section 長度 / scene offset /
//   指令76 跳轉全部不變, 不需重新序列化。
//
//   與 EEX.cs 的差異: 不建 Scene/Section/Command 物件樹、不解析指令76 跳轉、
//   不重序列化、不輸出文字位置、不走 Big5 —— 只用一個游標走 buffer 並就地改字。
//
//   安全: 全程以實際 bytesRead 為界做邊界檢查; 遇未知指令或結構越界即安全中止
//   該層走訪 (已套用的等長修改皆合法); 轉換後長度若 ≠ 原文則保留原文不蓋。
//*****************************************************

namespace Eex
{
    constexpr size_t HeaderLength = 10;

    // 指令型別 (自參考專案 CaoCaoEditor 的 Enum/CommandType.cs 原樣移植, R/S 劇本共用)。
    // 順序即編碼值; 每區段開頭以 = 0x?0 對齊, 與來源一致, 便於對照稽核。
    enum CommandType : unsigned short
    {
        // 0x00~0x0f
        事件結束 = 0x00, 子事件設定, 內部信息, ELSE, 詢問測試, 變量測試, 我軍出場限制, 出場測試,
        菜單處理, 延時, 初始化局部變量, 變量賦值, 結束Section, 結束Scene, 戰鬥失敗, 結局設定,
        // 0x10~0x1f
        許子將指導 = 0x10, 劇本跳轉, 選擇框, CASE, 對話, 對話2, 信息, 場所名,
        事件名稱設定, 勝利條件, 顯示勝利條件, 撤退信息是否顯示設定, 繪圖, 調色板設定, 武將重繪, 地圖視點切換,
        // 0x20~0x2f
        武將頭向狀態設置 = 0x20, 戰場物體添加, 動畫, 音效, CD音軌, 武將進入指定地點測試, 武將進入指定區域測試, 背景顯示,
        自由R啟動指令, 地圖頭像顯示, 地圖頭像移動, 地圖頭像消失, 地圖文字顯示, 武將點擊測試, 武將相鄰測試, 清除人物,
        // 0x30~0x3f
        武將出現 = 0x30, 武將消失, 武將移動, 武將轉向, 武將動作, 武將形象改變, 武將狀態測試, 錢_劇本跳轉_忠奸測試,
        武將能力設定, 武將等級提升, 錢_劇本跳轉_忠奸設置, 武將加入, 武將加入測試, 獲得物品, 加入裝備設定, 回合測試,
        // 0x40~0x4f
        行動方測試 = 0x40, 戰場人數測試, 戰鬥勝利測試, 戰鬥失敗測試, 戰鬥初始化, 戰場全局變量, 友軍出場設定, 敵軍出場設定,
        敵方裝備設定, 戰鬥結束, 我軍出場強制設定, 我軍出場設定, 隱藏武將出現, 武將狀態變更, 武將方針變更, 戰場轉向設置,
        // 0x50~0x5f
        戰場動作設定 = 0x50, 戰場恢復行動權, 兵種改變, 戰場撤退, 戰場撤退確認, 戰場復活, 天氣類別設定, 當前天氣設定,
        戰場障礙設定, 戰利品, 戰場操作開始, 戰場高亮區域, 戰場高亮人物, 回合上限設定, 武將不同測試, 單挑結束,
        // 0x60~0x6f
        單挑武將出場 = 0x60, 單挑勝負顯示, 單挑陣亡, 單挑對話, 單挑動作, 單挑攻擊1, 單挑攻擊2, 章名,
        單挑開始, 旁白, GameOver指令, 法術, 武將能力複製, 相對復活或移動, 概率測試, 丟棄物品,
        // 0x70~0x7f
        能力複製選擇 = 0x70, 特效請求, 信息傳送, 人物五圍和測試, 開or禁存檔, S特殊形象指定, 無條件跳轉, 變量運算,
        整型變量賦值, 變量測試2,
    };

    // 小端讀取 (含邊界檢查); 越界回傳 false。
    inline bool ReadU16(const BYTE* buf, size_t size, size_t pos, UINT16& out)
    {
        if (pos + 2 > size) return false;
        out = (UINT16)(buf[pos] | (buf[pos + 1] << 8));
        return true;
    }
    inline bool ReadU32(const BYTE* buf, size_t size, size_t pos, UINT32& out)
    {
        if (pos + 4 > size) return false;
        out = (UINT32)(buf[pos] | (buf[pos + 1] << 8) | (buf[pos + 2] << 16) | ((UINT32)buf[pos + 3] << 24));
        return true;
    }

    // 文字指令的 header/tail 長度; 非文字指令回傳 isText=false。
    struct TextLayout { bool isText; int header; int tail; };
    inline TextLayout GetTextLayout(UINT16 t)
    {
        switch (t)
        {
        // header=2, 無 tail
        case 內部信息: case 對話: case 信息: case 場所名:
        case 事件名稱設定: case 勝利條件: case 顯示勝利條件: case 旁白:
            return { true, 2, 0 };
        case 章名: case 信息傳送:        return { true, 8, 0 };
        case 對話2:                      return { true, 10, 0 };
        case 地圖文字顯示:               return { true, 2, 12 };
        case 選擇框:                     return { true, 2, 4 };
        case 單挑武將出場: case 單挑對話: return { true, 6, 4 };
        default:                         return { false, 0, 0 };
        }
    }

    // 非文字指令在「指令型別 2 bytes 之後」還佔幾 bytes。
    //   回傳 >=0 = 固定長度資料; -1 = 無參數 (只有型別); -2 = 變量測試(0x05,變動長度);
    //   -3 = 未知指令 (應中止)。
    inline int GetFixedDataLength(UINT16 t)
    {
        switch (t)
        {
        // 無參數 (只有 2 bytes 型別)
        case 事件結束: case 子事件設定: case ELSE: case 出場測試:
        case 初始化局部變量: case 結束Section: case 結束Scene: case 戰鬥失敗:
        case 繪圖: case 調色板設定: case 武將重繪: case 清除人物:
        case 戰鬥勝利測試: case 戰鬥失敗測試: case 戰鬥初始化: case 戰鬥結束:
        case 戰場恢復行動權: case 戰場撤退確認: case 戰場操作開始: case 單挑結束:
        case 單挑勝負顯示: case GameOver指令:
            return -1;

        case 變量測試: return -2;                      // 變動長度
        case 許子將指導: case 人物五圍和測試: return -3; // 未知, 安全中止

        // 固定長度資料 (型別之後再佔幾 bytes)
        case 詢問測試: case 菜單處理: case 結局設定: case 劇本跳轉: case 武將頭向狀態設置: case 動畫:
        case CD音軌: case 自由R啟動指令: case 地圖頭像消失: case 武將點擊測試: case 行動方測試: case 天氣類別設定:
        case 當前天氣設定: case 戰場高亮人物: case 單挑陣亡: case 丟棄物品: case 開or禁存檔:
            return 4;
        case 延時: case CASE: case 概率測試: case 特效請求: case 無條件跳轉:
            return 6;
        case 撤退信息是否顯示設定: case 武將動作: case 武將形象改變: case 兵種改變: case 武將不同測試: case 單挑動作:
        case 武將能力複製: case 能力複製選擇: case S特殊形象指定:
            return 8;
        case 變量賦值: case 音效: case 武將等級提升: case 回合測試: case 回合上限設定:
            return 10;
        case 地圖視點切換: case 武將相鄰測試: case 武將轉向: case 武將加入: case 武將加入測試: case 單挑攻擊1:
        case 單挑攻擊2: case 單挑開始:
            return 12;
        case 錢_劇本跳轉_忠奸測試: case 錢_劇本跳轉_忠奸設置: case 隱藏武將出現:
            return 14;
        case 武將進入指定地點測試: case 地圖頭像顯示: case 地圖頭像移動: case 獲得物品: case 戰場動作設定:
            return 16;
        case 武將狀態測試: case 武將能力設定: case 整型變量賦值:
            return 18;
        case 背景顯示: case 法術:
            return 20;
        case 戰場物體添加: case 武將出現: case 加入裝備設定: case 敵方裝備設定: case 戰場轉向設置: case 變量運算: case 變量測試2:
            return 24;
        case 我軍出場設定:
            return 26;
        case 武將進入指定區域測試: case 戰場高亮區域:
            return 28;
        case 武將移動: case 戰場復活:
            return 30;
        case 戰場障礙設定:
            return 32;
        case 戰利品:
            return 34;
        case 武將消失:
            return 36;
        case 相對復活或移動:
            return 38;
        case 戰場撤退:
            return 40;
        case 戰場人數測試: case 戰場全局變量:
            return 42;
        case 我軍出場強制設定:
            return 46;
        case 我軍出場限制:
            return 50;
        case 武將方針變更:
            return 56;
        case 武將狀態變更:
            return 66;
        case 友軍出場設定:
            return 52 * 20;
        case 敵軍出場設定:
            return 56 * 80;
        default:
            return -3;       // 未列入 = 未知, 安全中止
        }
    }

    // 變量測試 (0x05): body = 兩組「35 00 <count:2> (count*2 bytes)」。
    //   cur 指向型別之後 (第一個 35 00)。算出 body 長度; 越界回傳 false。
    inline bool VarTestBodyLength(const BYTE* buf, size_t size, size_t cur, size_t& bodyLen)
    {
        UINT16 c1, c2;
        if (!ReadU16(buf, size, cur + 2, c1)) return false;
        size_t len1 = 4 + (size_t)c1 * 2;
        if (!ReadU16(buf, size, cur + len1 + 2, c2)) return false;
        size_t len2 = 4 + (size_t)c2 * 2;
        bodyLen = len1 + len2;
        return (cur + bodyLen <= size);
    }

    // 就地轉換單一 body run [from, to) 的 GBK 文字; 等長才覆寫, 否則保留原文。
    inline void ConvertRun(BYTE* buf, size_t from, size_t to)
    {
        if (to <= from) return;
        size_t len = to - from;
        std::string gbk((const char*)(buf + from), len);
        std::string cht;
        if (GBK_FullConvert(gbk.c_str(), cht) && cht.size() == len)
        {
            memcpy(buf + from, cht.data(), len);
#ifdef _DEBUG
            DebugLog("[EexReadFile] [before] %s\n", gbk.c_str());
            DebugLog("[EexReadFile] [after ] %s\n", cht.c_str());
#endif
        }
    }

    // 就地轉換 [start, nul) 的 GBK 文字。
    //   對話格式為 "&人名 0A 文字 0A ..."; "&人名" 是引擎拿來查 GBK 簡體名表的 key,
    //   轉繁會查不到, 故名字標記 (從 '&' 到下一個換行 0x0A) 一律不轉, 只轉 body。
    //   '&'(0x26) 與換行(0x0A) 皆 < 0x40, 不可能是 GBK 雙位元組的第二位元組,
    //   故可安全地逐位元組掃描切段, 切點也必落在字元邊界上。
    inline void ConvertTextRegion(BYTE* buf, size_t start, size_t nul)
    {
        size_t i = start;
        while (i < nul)
        {
            if (buf[i] == '&')
            {
                // 名字標記: 跳到下一個換行(或 nul) 為止, 不轉換
                size_t j = i + 1;
                while (j < nul && buf[j] != 0x0A) j++;
                i = j;
            }
            else
            {
                // body 段: 到下一個 '&' 或 nul 為止, 就地等長轉換
                size_t k = i;
                while (k < nul && buf[k] != '&') k++;
                ConvertRun(buf, i, k);
                i = k;
            }
        }
    }

    // 走訪一個 command 區塊 [base, base+len), 逢文字指令就地轉換。
    //   existSub: 是否為頂層 section (true) —— 影響 事件結束(0x00) 是否帶 sub-section。
    void WalkCommands(BYTE* buf, size_t size, size_t base, size_t len, bool existSub)
    {
        size_t end = base + len;
        if (end > size) end = size;        // 不信任結構長度, 夾到實際 buffer 內
        size_t cur = base;
        bool ziShijian = false;            // 子事件設定 旗標 (見 EEX.cs)

        while (cur < end)
        {
            UINT16 type;
            if (!ReadU16(buf, end, cur, type)) break;
            cur += 2;

            TextLayout tl = GetTextLayout(type);
            if (tl.isText)
            {
                size_t textStart = cur + tl.header;
                if (textStart > end) break;
                // 找字串終止 0x00
                size_t nul = textStart;
                while (nul < end && buf[nul] != 0x00) nul++;
                if (nul >= end) break;     // 沒找到終止符 = 結構壞了, 中止
                ConvertTextRegion(buf, textStart, nul);
                cur = nul + 1 + tl.tail;
            }
            else
            {
                int fixedLen = GetFixedDataLength(type);
                if (fixedLen == -3) break; // 未知指令, 安全中止本層
                else if (fixedLen == -2)   // 變量測試
                {
                    size_t bodyLen;
                    if (!VarTestBodyLength(buf, end, cur, bodyLen)) break;
                    cur += bodyLen;
                }
                else if (fixedLen >= 0)
                {
                    cur += (size_t)fixedLen;
                }
                // fixedLen == -1: 無參數, cur 不動
            }

            if (type == 子事件設定) ziShijian = true; // 標記下一指令帶 sub-section

            bool cond1 = existSub && type == 事件結束;                       // 頂層 事件結束 帶 sub-section
            bool cond2 = ziShijian && type != 事件結束 && type != 子事件設定; // 子事件設定 後的指令帶 sub-section
            if (cond1 || cond2)
            {
                ziShijian = false;
                UINT16 subLen;
                if (!ReadU16(buf, end, cur, subLen)) break;
                cur += 2;
                size_t subStart = cur;
                if (subStart + subLen > end) break;
                WalkCommands(buf, size, subStart, subLen, false);
                cur += subLen;
            }
        }
    }

    // 進入點: 對遊戲讀到的 eex buffer 就地簡轉繁。buffer 開頭須為 "EEX\0"。
    inline void ConvertBufferInPlace(BYTE* buf, DWORD size)
    {
        if (buf == nullptr || size < HeaderLength + 4) return;
        if (!(buf[0] == 'E' && buf[1] == 'E' && buf[2] == 'X' && buf[3] == 0x00)) return;

        // Scene offset 表: 第一個 offset 即表尾位置, 由此推得 scene 數。
        UINT32 firstOffset;
        if (!ReadU32(buf, size, HeaderLength, firstOffset)) return;
        if (firstOffset <= HeaderLength || firstOffset > size) return;
        if ((firstOffset - HeaderLength) % 4 != 0) return;
        UINT32 sceneCount = (firstOffset - (UINT32)HeaderLength) / 4;
        if (sceneCount == 0) return;

        size_t pos = firstOffset; // scene 資料起點 (緊接在 offset 表後)
        for (UINT32 i = 0; i < sceneCount; i++)
        {
            UINT16 sectionCount;
            if (!ReadU16(buf, size, pos, sectionCount)) return;
            pos += 2;
            for (UINT16 j = 0; j < sectionCount; j++)
            {
                UINT16 sectionLength;
                if (!ReadU16(buf, size, pos, sectionLength)) return;
                pos += 2;
                size_t sectionStart = pos;
                if (sectionStart + sectionLength > size) return;
                WalkCommands(buf, size, sectionStart, sectionLength, true);
                pos += sectionLength;
            }
        }
    }
}

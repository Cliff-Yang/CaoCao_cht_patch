# 曹操傳MOD 簡轉繁補丁

文章說明: https://codingnokiseki.blogspot.com/2022/10/mod.html

## 使用步驟
1. 將遊戲目錄底下的 Koeicda.dll 重新命名為 Koeicda_Origin.dll
2. 把 Koeicda.dll, dictionary.txt 複製到遊戲目錄下
    > 如果要搭配伯伯補丁同時使用的話，就要把 build 出來的 `Koeicda.dll` 改成 `Koeicda_Org.dll`
3. 使用簡體執行 Ekd5.exe
    - Win7 以前可以安裝 `Application Locale` 來執行 Ekd5.exe
    - Win10 要使用 `Locale Emulator` 執行 Ekd5.exe

## 支援的同引擎遊戲

同一遊戲引擎也被其他同人作品採用，本補丁已支援（`FullSentenceCall` 內建多組特徵碼、並依命中變體自動切換呼叫慣例）：

- 三國志曹操傳 MOD
- 天龍八部（新引擎）
- 云荒逍遥传

> 開發備註：移植到新作時若特徵碼掃不到，先確認該 exe 是否加殼（檔案巨大、imports 被砍、section 名稱亂碼），加殼的需用 `tools/mem_dump.py` 把執行中遊戲記憶體 dump 下來再分析。完整查因與多 sig／呼叫慣例的處理見 `notes/` 與 `CLAUDE.md`。

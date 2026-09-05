# MrpackMaker

Qt 6 GUI 的 Modrinth `.mrpack` 整合包製作工具。

## 功能

- 精靈式流程
- 輸入模組包基本資訊
- 透過直連網址或本機 `.jar` 匯入模組
- 內建搜尋 Modrinth 與 CurseForge
- 自動計算 `sha1`、`sha512` 與檔案大小
- 設定 `client`、`server` 與 `required / optional / unsupported`
- 管理 `overrides`、`client-overrides`、`server-overrides`
- 背景匯出並顯示進度

## 使用流程

1. 填寫基本資訊：
   - 名稱
   - 簡介
   - 版本號
   - Minecraft 版本
   - 載入器類型：Fabric、Forge、NeoForge 或 Quilt
   - 載入器版本

2. 新增檔案：
   - 可貼上一或多個直連網址
   - 可匯入本機 `.jar` 檔
   - 可直接搜尋 Modrinth 或 CurseForge 後加入清單

3. 計算雜湊與檔案大小：
   - 下載的檔案會暫存到 `%TEMP%\Mrpack\Mods\`
   - 本機檔案直接讀取
   - 程式會自動填入 `sha1`、`sha512` 與大小

4. 設定覆蓋資料夾：
   - 選擇實例資料夾
   - 勾選要包含的資料夾或檔案
   - 可分別設定全域、Client-only、Server-only

5. 匯出：
   - 先確認摘要
   - 選擇輸出路徑
   - 開始匯出後可繼續使用程式

## Modrinth / CurseForge 搜尋

- Modrinth 不需要 API Key。
- CurseForge 需要 API Key。
- 加入清單後，檔案會以一般直連項目的方式處理。

## CurseForge API Key 儲存

CurseForge API Key 會使用 Windows 專用的加密方式儲存。
私鑰會經 DPAPI 保護，儲存路徑會記錄在 Windows 登錄檔中。

## 建置方式

### 需求

- Qt 6.11.0
- MSVC 2022
- 手動安裝 QuaZip
- OpenSSL 可供 CMake 找到
- vcpkg 與 `zlib`

### 設定與建置

```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE=<vcpkg-path>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## 注意事項

- `.mrpack` 匯出使用標準 ZIP 的 Deflate 壓縮。
- 本程式僅支援 Windows。

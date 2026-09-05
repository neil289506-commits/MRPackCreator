#pragma once
#include <QObject>
#include <QString>
#include "ModpackData.h"

// 負責：
//   1. 依 ModpackData 組出符合規格的 modrinth.index.json
//   2. 用 QuaZip 把 index.json + overrides / client-overrides / server-overrides
//      打包成標準 ZIP，另存為 .mrpack
//
// 重要技術說明（請見對話回覆中的完整說明）：
// Modrinth 官方規格明訂 .mrpack 內部必須是「標準 ZIP」格式
// （MIME type application/x-modrinth-modpack+zip），也就是 entry 的壓縮方式
// 只能是 Store 或 Deflate。若改用 bzip2 當作 zip entry 的壓縮方法（method 12），
// 雖然 QuaZip/zlib 技術上支援寫入，但多數第三方啟動器（Prism Launcher、
// MultiMC、ATLauncher、Modrinth App 本身等）用的 zip 函式庫不見得支援解壓 bzip2
// entry，會導致其他人下載到的 .mrpack 無法被正常讀取/安裝。
// 因此這裡固定使用 Deflate 壓縮 zip entry，以確保相容性；
// vcpkg 裝的 bzip2 函式庫可以保留給專案裡其他需要 bz2 的地方使用，
// 但不建議套用在 .mrpack 本身的 zip 壓縮方法上。
//
// 進度回報採「總進度 / 階段進度 / 目前階段文字」三段式：
//   - stageChanged：總進度，目前是第幾個階段（寫入 index.json / 加入 overrides.../加入 client-overrides... 等）
//   - stageProgress：階段進度，目前這個階段內已處理幾個檔案
//   - progress：逐行文字說明，給 Console 顯示用
class MrpackBuilder : public QObject
{
    Q_OBJECT
public:
    explicit MrpackBuilder(QObject *parent = nullptr);

    QByteArray buildIndexJson(const ModpackData &data) const;

    // 執行完整匯出流程，outputPath 例如 "D:/xxx/MyPack-1.0.0.mrpack"
    // 回傳是否成功；失敗時 errorOut 會填入原因。
    bool exportMrpack(const ModpackData &data, const QString &outputPath, QString &errorOut);

signals:
    void progress(const QString &message);                                  // 逐行文字（Console）
    void stageChanged(int currentStage, int totalStages, const QString &stageName); // 總進度
    void stageProgress(int current, int total);                             // 階段進度

private:
    bool addPathToZip(class QuaZip &zip, const QString &absPath, const QString &zipPath,
                       int &doneCounter, int totalCounter, QString &errorOut);
    bool addSelectionToZip(class QuaZip &zip, const OverrideSelection &sel, const QString &zipPrefix,
                            int &doneCounter, int totalCounter, QString &errorOut);
    int countPath(const QString &absPath) const;
    int countSelection(const OverrideSelection &sel) const;
};

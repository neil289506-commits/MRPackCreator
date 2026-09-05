#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include "../ModloaderInfo.h"

enum class EnvSupport { Required, Optional, Unsupported };

inline QString envToString(EnvSupport e)
{
    switch (e) {
    case EnvSupport::Required:    return QStringLiteral("required");
    case EnvSupport::Optional:    return QStringLiteral("optional");
    case EnvSupport::Unsupported: return QStringLiteral("unsupported");
    }
    return QStringLiteral("required");
}

// 對應 modrinth.index.json -> files[] 的單一項目
struct ModFileEntry {
    QString path;          // 例如 "mods/sodium.jar"（相對於 Minecraft 實例目錄）
    QStringList downloads; // 一或多個下載網址（直連來源就是原網址；本機檔案則需使用者另外提供可下載網址，
                            // 若沒有網址，Modrinth 規格仍要求至少一個 downloads URL，
                            // 因此本機匯入的檔案在 UI 上會提示使用者補上可公開下載的網址）
    QString sha1;
    QString sha512;
    qint64  fileSize = 0;

    EnvSupport clientEnv = EnvSupport::Required;
    EnvSupport serverEnv = EnvSupport::Required;

    // UI/處理用暫存欄位，不會寫入最終 json
    QString localTempPath;   // 本機檔案路徑，或直連下載後的暫存路徑
    bool    isLocalImport = false;
    bool    hashComputed = false;
    QString statusText;      // 顯示於表格「狀態」欄
};

// 一個 overrides 類別（全局 / client-only / server-only）的使用者選擇。
// 使用者先選擇一個「實例（instance）資料夾」（例如 Prism/MultiMC 的某個 instance 的
// .minecraft 資料夾，或任何自己整理好的模組包工作目錄），而不是直接假設整個資料夾
// 都要打包；再用勾選樹狀清單選出實際要包含的檔案/資料夾。
struct OverrideSelection {
    QString instanceDir;        // 使用者選擇的實例根目錄（可空）
    QStringList selectedPaths;  // 勾選、相對於 instanceDir 的路徑清單；
                                 // 若某路徑是資料夾，代表整個資料夾（含子項目）都要包含，
                                 // 不需要再列出其底下每個檔案
};

struct OverridesConfig {
    OverrideSelection overrides;       // 全局 overrides（可空）
    OverrideSelection clientOverrides; // 僅客戶端（可空）
    OverrideSelection serverOverrides; // 僅伺服器端（可空）
};

struct ModpackBasicInfo {
    QString name;
    QString summary;
    QString versionId;
    QString mcVersion;
    LoaderType loaderType = LoaderType::Fabric;
    QString loaderVersion;
};

struct ModpackData {
    ModpackBasicInfo basic;
    QVector<ModFileEntry> files;
    OverridesConfig overrides;
};

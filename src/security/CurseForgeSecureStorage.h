#pragma once
#include <QString>

// 整合 RsaCrypto + RegistryKeyStore + KeyPathGenerator，提供
// CurseForge API Key 的完整安全儲存流程：
//   開機（首次）：產生 RSA 金鑰對 → 存到隨機路徑 → 路徑記錄進登錄檔
//   開機（之後）：從登錄檔讀路徑 → 讀出金鑰檔內容
//   儲存 Key：用公鑰加密 → 寫入 %APPDATA%\ServerManager\CurseforgeAPI.inf
//   讀取 Key：讀密文 → 用私鑰解密 → 回傳明文
//
// 這一層是 MainWindow 唯一需要呼叫的介面，不用管底層 RSA 細節。
class CurseForgeSecureStorage
{
public:
    // 確保金鑰對存在（首次呼叫時會自動產生並記錄到登錄檔，
    // 之後呼叫直接讀已存在的路徑）。程式啟動時呼叫一次即可。
    // 失敗回傳 false 並可透過 RsaCrypto::lastError() / 自身的 lastError() 查看原因。
    static bool ensureKeyPairExists();

    // 儲存 API Key（會自動用公鑰加密後寫入 .inf 檔）
    static bool saveApiKey(const QString &apiKey);

    // 讀取並解密 API Key，檔案不存在或解密失敗回傳空字串
    static QString loadApiKey();

    // 清除已儲存的 API Key（刪除 .inf 檔內容，不影響金鑰對本身）
    static bool clearApiKey();

    static QString lastError();

private:
    static QString iniFilePath(); // %APPDATA%\ServerManager\CurseforgeAPI.inf
    static void setLastError(const QString &msg);
};

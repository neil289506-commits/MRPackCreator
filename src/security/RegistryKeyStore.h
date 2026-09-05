#pragma once
#include <QString>

// 把 RSA 金鑰檔的路徑「藏」在登錄檔裡，登錄檔的數值名稱也隨機產生，
// 只在首次啟動時建立一次，之後直接讀取。這只防止隨手翻找，不是
// 密碼學等級的保護。
class RegistryKeyStore
{
public:
    static bool tryLoadKeyPaths(QString &outPrivateKeyPath, QString &outPublicKeyPath);
    static bool saveKeyPaths(const QString &privateKeyPath, const QString &publicKeyPath);

private:
    static QString registryRootPath();
    static QString generateRandomValueName();
};

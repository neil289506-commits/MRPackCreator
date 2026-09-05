#include "CurseForgeSecureStorage.h"
#include "RsaCrypto.h"
#include "RegistryKeyStore.h"
#include "KeyPathGenerator.h"
#include "DpapiProtector.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QSettings>

namespace {
QString g_lastError;
QString g_privateKeyPem;
QString g_publicKeyPem;
bool g_keysLoaded = false;
}

void CurseForgeSecureStorage::setLastError(const QString &msg) { g_lastError = msg; }
QString CurseForgeSecureStorage::lastError() { return g_lastError; }

QString CurseForgeSecureStorage::iniFilePath()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return QDir(appData).filePath("CurseforgeAPI.inf");
}

// 公鑰不是機密（依定義就是要公開的），直接明文讀寫即可，
// 沒有 DPAPI 保護的必要。
static bool readPublicKeyFile(const QString &path, QString &out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    out = in.readAll();
    return true;
}
static bool writePublicKeyFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    return true;
}

// 私鑰是整套機制真正的機密：寫檔前先用 DPAPI 加密，讀檔後先用 DPAPI 解密，
// 檔案本身在磁碟上看起來就是一坨亂數 bytes，就算被複製走也沒用
// （綁定這台電腦這個 Windows 帳號才解得開）。
static bool readPrivateKeyFileProtected(const QString &path, QString &out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray protectedBytes = f.readAll();
    f.close();
    if (protectedBytes.isEmpty()) return false;

    const QByteArray plainBytes = DpapiProtector::unprotect(protectedBytes);
    if (plainBytes.isEmpty()) return false;

    out = QString::fromUtf8(plainBytes);
    return true;
}
static bool writePrivateKeyFileProtected(const QString &path, const QString &content)
{
    const QByteArray protectedBytes = DpapiProtector::protect(content.toUtf8());
    if (protectedBytes.isEmpty()) return false;

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(protectedBytes);
    return true;
}

bool CurseForgeSecureStorage::ensureKeyPairExists()
{
    if (g_keysLoaded) return true;

    QString privPath, pubPath;
    if (RegistryKeyStore::tryLoadKeyPaths(privPath, pubPath)) {
        if (readPrivateKeyFileProtected(privPath, g_privateKeyPem) &&
            readPublicKeyFile(pubPath, g_publicKeyPem) &&
            !g_privateKeyPem.isEmpty() && !g_publicKeyPem.isEmpty()) {
            g_keysLoaded = true;
            return true;
        }
        // 讀取/解密失敗的兩種可能：檔案遺失，或者這把私鑰是在別台電腦/
        // 別的 Windows 帳號下產生的（DPAPI 解不開）。不管哪種情況，
        // 現有密文都已經無法使用，只能重新產生一組新的金鑰對。
        setLastError("無法讀取或解密既有的金鑰檔案（可能是檔案遺失，或此金鑰"
                      "是在別的電腦/Windows 帳號下產生的），將重新產生新的金鑰對。"
                      "注意：先前用舊金鑰加密存起來的 API Key 將無法解密，需要重新輸入。");
    }

    if (!RsaCrypto::generateKeyPair(g_privateKeyPem, g_publicKeyPem)) {
        setLastError("產生 RSA 金鑰對失敗：" + RsaCrypto::lastError());
        return false;
    }

    QString newPrivPath, newPubPath;
    KeyPathGenerator::generatePaths(newPrivPath, newPubPath);

    if (!writePrivateKeyFileProtected(newPrivPath, g_privateKeyPem)) {
        setLastError("寫入並加密私鑰檔案失敗（DPAPI 保護失敗），路徑：" + newPrivPath);
        return false;
    }
    if (!writePublicKeyFile(newPubPath, g_publicKeyPem)) {
        setLastError("寫入公鑰檔案失敗，路徑：" + newPubPath);
        return false;
    }
    if (!RegistryKeyStore::saveKeyPaths(newPrivPath, newPubPath)) {
        setLastError("寫入登錄檔記錄失敗");
        return false;
    }

    g_keysLoaded = true;
    return true;
}

bool CurseForgeSecureStorage::saveApiKey(const QString &apiKey)
{
    if (!ensureKeyPairExists()) return false;
    if (apiKey.trimmed().isEmpty()) return clearApiKey();

    const QString cipherText = RsaCrypto::encryptWithPublicKey(g_publicKeyPem, apiKey.trimmed());
    if (cipherText.isEmpty()) {
        setLastError("加密失敗：" + RsaCrypto::lastError());
        return false;
    }

    QSettings ini(iniFilePath(), QSettings::IniFormat);
    ini.setValue("data", cipherText);
    ini.sync();
    return ini.status() == QSettings::NoError;
}

QString CurseForgeSecureStorage::loadApiKey()
{
    if (!ensureKeyPairExists()) return {};

    QSettings ini(iniFilePath(), QSettings::IniFormat);
    const QString cipherText = ini.value("data").toString();
    if (cipherText.isEmpty()) return {};

    const QString plain = RsaCrypto::decryptWithPrivateKey(g_privateKeyPem, cipherText);
    if (plain.isEmpty()) setLastError("解密失敗：" + RsaCrypto::lastError());
    return plain;
}

bool CurseForgeSecureStorage::clearApiKey()
{
    QSettings ini(iniFilePath(), QSettings::IniFormat);
    ini.remove("data");
    ini.sync();
    return ini.status() == QSettings::NoError;
}


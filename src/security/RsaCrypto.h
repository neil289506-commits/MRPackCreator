#pragma once
#include <QString>
#include <QByteArray>

// 包裝 OpenSSL EVP_PKEY 高階 API：
//   1. 產生 RSA 2048-bit 金鑰對，輸出 PEM 格式字串
//   2. 混合加密（RSA-OAEP 加密隨機 AES 金鑰 + AES-256-GCM 加密實際內容）
//      而不是直接用 RSA 加密整段明文 —— RSA-2048 單次加密上限約 190 bytes
//      （OAEP padding 開銷），CurseForge API Key 通常很短夠用，但混合加密
//      是業界標準做法，不受長度限制，未來要存更長的機密資料也不用改架構。
//   3. 對應的解密流程
//
// 所有函式失敗時回傳空值/false，可透過 lastError() 取得錯誤訊息。
class RsaCrypto
{
public:
    static bool generateKeyPair(QString &outPrivateKeyPem, QString &outPublicKeyPem);

    // 回傳 Base64 編碼的密文（RSA 加密的 AES 金鑰 + IV + AES-GCM 密文 + Tag 打包在一起）
    static QString encryptWithPublicKey(const QString &publicKeyPem, const QString &plainText);

    static QString decryptWithPrivateKey(const QString &privateKeyPem, const QString &cipherTextBase64);

    static QString lastError();

private:
    static void setLastError(const QString &msg);
};

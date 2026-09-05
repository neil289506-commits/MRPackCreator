#include "RsaCrypto.h"
#include <QIODevice>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <QByteArray>
#include <QDataStream>

namespace {
QString g_lastError;

// AES-256-GCM 參數：金鑰 32 bytes、IV 12 bytes（GCM 建議長度）、
// 認證 Tag 16 bytes（標準 GCM tag 長度）。
constexpr int kAesKeyLen = 32;
constexpr int kAesIvLen  = 12;
constexpr int kGcmTagLen = 16;

void captureOpenSslError()
{
    unsigned long err = ERR_get_error();
    if (err == 0) { g_lastError = "未知的 OpenSSL 錯誤"; return; }
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    g_lastError = QString::fromLatin1(buf);
}
} // namespace

void RsaCrypto::setLastError(const QString &msg) { g_lastError = msg; }
QString RsaCrypto::lastError() { return g_lastError; }

bool RsaCrypto::generateKeyPair(QString &outPrivateKeyPem, QString &outPublicKeyPem)
{
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) { captureOpenSslError(); return false; }

    bool ok = false;
    EVP_PKEY *pkey = nullptr;
    BIO *privBio = nullptr;
    BIO *pubBio  = nullptr;

    do {
        if (EVP_PKEY_keygen_init(ctx) <= 0) { captureOpenSslError(); break; }
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) { captureOpenSslError(); break; }
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0) { captureOpenSslError(); break; }

        privBio = BIO_new(BIO_s_mem());
        pubBio  = BIO_new(BIO_s_mem());
        if (!privBio || !pubBio) { setLastError("BIO_new 失敗"); break; }

        if (PEM_write_bio_PrivateKey(privBio, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
            captureOpenSslError(); break;
        }
        if (PEM_write_bio_PUBKEY(pubBio, pkey) != 1) {
            captureOpenSslError(); break;
        }

        char *privData = nullptr;
        char *pubData  = nullptr;
        const long privLen = BIO_get_mem_data(privBio, &privData);
        const long pubLen  = BIO_get_mem_data(pubBio, &pubData);
        if (privLen <= 0 || pubLen <= 0) { setLastError("讀取 PEM 記憶體緩衝區失敗"); break; }

        outPrivateKeyPem = QString::fromLatin1(privData, static_cast<int>(privLen));
        outPublicKeyPem  = QString::fromLatin1(pubData,  static_cast<int>(pubLen));
        ok = true;
    } while (false);

    if (pkey)   EVP_PKEY_free(pkey);
    if (privBio) BIO_free(privBio);
    if (pubBio)  BIO_free(pubBio);
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

QString RsaCrypto::encryptWithPublicKey(const QString &publicKeyPem, const QString &plainText)
{
    if (publicKeyPem.isEmpty() || plainText.isEmpty()) {
        setLastError("公鑰或明文為空");
        return {};
    }

    const QByteArray pubPemBytes = publicKeyPem.toLatin1();
    BIO *pubBio = BIO_new_mem_buf(pubPemBytes.constData(), pubPemBytes.size());
    if (!pubBio) { setLastError("BIO_new_mem_buf 失敗"); return {}; }

    EVP_PKEY *pkey = PEM_read_bio_PUBKEY(pubBio, nullptr, nullptr, nullptr);
    BIO_free(pubBio);
    if (!pkey) { captureOpenSslError(); return {}; }

    QString result;
    EVP_PKEY_CTX *rsaCtx = nullptr;
    EVP_CIPHER_CTX *aesCtx = nullptr;

    do {
        // ---- 步驟 1：產生隨機 AES-256 金鑰 + IV ----
        QByteArray aesKey(kAesKeyLen, 0);
        QByteArray iv(kAesIvLen, 0);
        if (RAND_bytes(reinterpret_cast<unsigned char*>(aesKey.data()), kAesKeyLen) != 1 ||
            RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), kAesIvLen) != 1) {
            captureOpenSslError(); break;
        }

        // ---- 步驟 2：用 RSA-OAEP 公鑰加密這把 AES 金鑰 ----
        rsaCtx = EVP_PKEY_CTX_new(pkey, nullptr);
        if (!rsaCtx || EVP_PKEY_encrypt_init(rsaCtx) <= 0 ||
            EVP_PKEY_CTX_set_rsa_padding(rsaCtx, RSA_PKCS1_OAEP_PADDING) <= 0) {
            captureOpenSslError(); break;
        }
        size_t encKeyLen = 0;
        if (EVP_PKEY_encrypt(rsaCtx, nullptr, &encKeyLen,
                reinterpret_cast<const unsigned char*>(aesKey.constData()), kAesKeyLen) <= 0) {
            captureOpenSslError(); break;
        }
        QByteArray encAesKey(static_cast<int>(encKeyLen), 0);
        if (EVP_PKEY_encrypt(rsaCtx, reinterpret_cast<unsigned char*>(encAesKey.data()), &encKeyLen,
                reinterpret_cast<const unsigned char*>(aesKey.constData()), kAesKeyLen) <= 0) {
            captureOpenSslError(); break;
        }
        encAesKey.resize(static_cast<int>(encKeyLen));

        // ---- 步驟 3：用 AES-256-GCM 加密實際明文 ----
        const QByteArray plainBytes = plainText.toUtf8();
        aesCtx = EVP_CIPHER_CTX_new();
        if (!aesCtx || EVP_EncryptInit_ex(aesCtx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
            EVP_EncryptInit_ex(aesCtx, nullptr, nullptr,
                reinterpret_cast<const unsigned char*>(aesKey.constData()),
                reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
            captureOpenSslError(); break;
        }

        QByteArray cipherText(plainBytes.size() + 16, 0); // 留餘裕，GCM 是串流密碼實際長度等於明文長度
        int outLen = 0, totalLen = 0;
        if (EVP_EncryptUpdate(aesCtx, reinterpret_cast<unsigned char*>(cipherText.data()), &outLen,
                reinterpret_cast<const unsigned char*>(plainBytes.constData()), plainBytes.size()) != 1) {
            captureOpenSslError(); break;
        }
        totalLen = outLen;
        if (EVP_EncryptFinal_ex(aesCtx, reinterpret_cast<unsigned char*>(cipherText.data()) + totalLen, &outLen) != 1) {
            captureOpenSslError(); break;
        }
        totalLen += outLen;
        cipherText.resize(totalLen);

        QByteArray tag(kGcmTagLen, 0);
        if (EVP_CIPHER_CTX_ctrl(aesCtx, EVP_CTRL_GCM_GET_TAG, kGcmTagLen, tag.data()) != 1) {
            captureOpenSslError(); break;
        }

        // ---- 步驟 4：打包成單一 Base64 字串 ----
        // 格式：[4 bytes encAesKey長度][encAesKey][12 bytes iv][16 bytes tag][cipherText]
        // 用 QDataStream 手動組裝，長度前綴用固定 4 bytes big-endian，
        // 解密端照同樣順序拆解即可，不依賴任何額外的分隔符號。
        QByteArray packed;
        QDataStream stream(&packed, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);
        stream << static_cast<quint32>(encAesKey.size());
        stream.writeRawData(encAesKey.constData(), encAesKey.size());
        stream.writeRawData(iv.constData(), iv.size());
        stream.writeRawData(tag.constData(), tag.size());
        stream.writeRawData(cipherText.constData(), cipherText.size());

        result = QString::fromLatin1(packed.toBase64());
    } while (false);

    if (rsaCtx) EVP_PKEY_CTX_free(rsaCtx);
    if (aesCtx) EVP_CIPHER_CTX_free(aesCtx);
    EVP_PKEY_free(pkey);
    return result;
}

QString RsaCrypto::decryptWithPrivateKey(const QString &privateKeyPem, const QString &cipherTextBase64)
{
    if (privateKeyPem.isEmpty() || cipherTextBase64.isEmpty()) {
        setLastError("私鑰或密文為空");
        return {};
    }

    const QByteArray privPemBytes = privateKeyPem.toLatin1();
    BIO *privBio = BIO_new_mem_buf(privPemBytes.constData(), privPemBytes.size());
    if (!privBio) { setLastError("BIO_new_mem_buf 失敗"); return {}; }

    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(privBio, nullptr, nullptr, nullptr);
    BIO_free(privBio);
    if (!pkey) { captureOpenSslError(); return {}; }

    QString result;
    EVP_PKEY_CTX *rsaCtx = nullptr;
    EVP_CIPHER_CTX *aesCtx = nullptr;

    do {
        // ---- 拆解打包格式 ----
        const QByteArray packed = QByteArray::fromBase64(cipherTextBase64.toLatin1());
        QDataStream stream(packed);
        stream.setByteOrder(QDataStream::BigEndian);

        quint32 encKeyLen = 0;
        stream >> encKeyLen;
        if (stream.status() != QDataStream::Ok || encKeyLen == 0 || encKeyLen > 4096) {
            setLastError("密文格式無效（金鑰長度欄位異常）");
            break;
        }

        QByteArray encAesKey(static_cast<int>(encKeyLen), 0);
        if (stream.readRawData(encAesKey.data(), encKeyLen) != static_cast<int>(encKeyLen)) {
            setLastError("密文格式無效（讀取加密金鑰失敗）"); break;
        }
        QByteArray iv(kAesIvLen, 0);
        if (stream.readRawData(iv.data(), kAesIvLen) != kAesIvLen) {
            setLastError("密文格式無效（讀取 IV 失敗）"); break;
        }
        QByteArray tag(kGcmTagLen, 0);
        if (stream.readRawData(tag.data(), kGcmTagLen) != kGcmTagLen) {
            setLastError("密文格式無效（讀取認證 Tag 失敗）"); break;
        }
        // 剩下的都是實際密文
        const QByteArray cipherText = packed.mid(4 + encKeyLen + kAesIvLen + kGcmTagLen);
        if (cipherText.isEmpty()) {
            setLastError("密文格式無效（實際密文為空）"); break;
        }

        // ---- 步驟 1：RSA 私鑰解密回 AES 金鑰 ----
        rsaCtx = EVP_PKEY_CTX_new(pkey, nullptr);
        if (!rsaCtx || EVP_PKEY_decrypt_init(rsaCtx) <= 0 ||
            EVP_PKEY_CTX_set_rsa_padding(rsaCtx, RSA_PKCS1_OAEP_PADDING) <= 0) {
            captureOpenSslError(); break;
        }
        size_t aesKeyLen = 0;
        if (EVP_PKEY_decrypt(rsaCtx, nullptr, &aesKeyLen,
                reinterpret_cast<const unsigned char*>(encAesKey.constData()), encAesKey.size()) <= 0) {
            captureOpenSslError(); break;
        }
        QByteArray aesKey(static_cast<int>(aesKeyLen), 0);
        if (EVP_PKEY_decrypt(rsaCtx, reinterpret_cast<unsigned char*>(aesKey.data()), &aesKeyLen,
                reinterpret_cast<const unsigned char*>(encAesKey.constData()), encAesKey.size()) <= 0) {
            captureOpenSslError(); break;
        }
        aesKey.resize(static_cast<int>(aesKeyLen));
        if (aesKey.size() != kAesKeyLen) {
            setLastError("解密出的 AES 金鑰長度異常，密文可能已損毀或金鑰不匹配");
            break;
        }

        // ---- 步驟 2：AES-256-GCM 解密實際內容 ----
        aesCtx = EVP_CIPHER_CTX_new();
        if (!aesCtx || EVP_DecryptInit_ex(aesCtx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
            EVP_DecryptInit_ex(aesCtx, nullptr, nullptr,
                reinterpret_cast<const unsigned char*>(aesKey.constData()),
                reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
            captureOpenSslError(); break;
        }

        QByteArray plainText(cipherText.size(), 0);
        int outLen = 0, totalLen = 0;
        if (EVP_DecryptUpdate(aesCtx, reinterpret_cast<unsigned char*>(plainText.data()), &outLen,
                reinterpret_cast<const unsigned char*>(cipherText.constData()), cipherText.size()) != 1) {
            captureOpenSslError(); break;
        }
        totalLen = outLen;

        if (EVP_CIPHER_CTX_ctrl(aesCtx, EVP_CTRL_GCM_SET_TAG, kGcmTagLen,
                const_cast<char*>(tag.constData())) != 1) {
            captureOpenSslError(); break;
        }

        // EVP_DecryptFinal_ex 回傳 <=0 代表 GCM 認證 Tag 驗證失敗，
        // 即密文在傳輸/儲存過程中被竄改過（或用錯私鑰解），這是 GCM
        // 模式提供的完整性保護，比純 AES-CBC 多了防篡改能力。
        if (EVP_DecryptFinal_ex(aesCtx, reinterpret_cast<unsigned char*>(plainText.data()) + totalLen, &outLen) != 1) {
            setLastError("解密驗證失敗：密文可能已被竄改，或使用了錯誤的金鑰");
            break;
        }
        totalLen += outLen;
        plainText.resize(totalLen);

        result = QString::fromUtf8(plainText);
    } while (false);

    if (rsaCtx) EVP_PKEY_CTX_free(rsaCtx);
    if (aesCtx) EVP_CIPHER_CTX_free(aesCtx);
    EVP_PKEY_free(pkey);
    return result;
}

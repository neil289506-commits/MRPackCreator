#include "HashCalculator.h"
#include <QFile>
#include <openssl/evp.h>

namespace {

QString toHex(const unsigned char *data, unsigned int len)
{
    static const char *hexDigits = "0123456789abcdef";
    QString out;
    out.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        out.append(hexDigits[(data[i] >> 4) & 0xF]);
        out.append(hexDigits[data[i] & 0xF]);
    }
    return out;
}

// 用單一 EVP_MD_CTX 邊讀邊算，同一輪檔案讀取同時餵給 sha1 與 sha512 兩個 context，
// 避免讀取檔案兩次。
bool hashFile(const QString &path, QString &sha1Out, QString &sha512Out, qint64 &sizeOut, QString &errOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        errOut = QObject::tr("無法開啟檔案：%1").arg(f.errorString());
        return false;
    }

    EVP_MD_CTX *ctx1 = EVP_MD_CTX_new();
    EVP_MD_CTX *ctx512 = EVP_MD_CTX_new();
    if (!ctx1 || !ctx512) {
        if (ctx1) EVP_MD_CTX_free(ctx1);
        if (ctx512) EVP_MD_CTX_free(ctx512);
        errOut = QObject::tr("無法建立 OpenSSL EVP 上下文");
        return false;
    }

    bool ok = EVP_DigestInit_ex(ctx1, EVP_sha1(), nullptr) == 1
           && EVP_DigestInit_ex(ctx512, EVP_sha512(), nullptr) == 1;

    constexpr qint64 kChunkSize = 1 << 20; // 1 MiB
    QByteArray buffer;
    buffer.resize(kChunkSize);

    qint64 total = 0;
    while (ok) {
        const qint64 n = f.read(buffer.data(), buffer.size());
        if (n < 0) {
            errOut = QObject::tr("讀取檔案時發生錯誤：%1").arg(f.errorString());
            ok = false;
            break;
        }
        if (n == 0)
            break;
        total += n;
        ok = EVP_DigestUpdate(ctx1, buffer.constData(), static_cast<size_t>(n)) == 1
          && EVP_DigestUpdate(ctx512, buffer.constData(), static_cast<size_t>(n)) == 1;
    }

    unsigned char digest1[EVP_MAX_MD_SIZE];
    unsigned char digest512[EVP_MAX_MD_SIZE];
    unsigned int len1 = 0, len512 = 0;

    if (ok) {
        ok = EVP_DigestFinal_ex(ctx1, digest1, &len1) == 1
          && EVP_DigestFinal_ex(ctx512, digest512, &len512) == 1;
    }

    EVP_MD_CTX_free(ctx1);
    EVP_MD_CTX_free(ctx512);

    if (!ok) {
        if (errOut.isEmpty())
            errOut = QObject::tr("OpenSSL 雜湊計算失敗");
        return false;
    }

    sha1Out = toHex(digest1, len1);
    sha512Out = toHex(digest512, len512);
    sizeOut = total;
    return true;
}

} // namespace

FileHashResult HashCalculator::calculate(const QString &localFilePath)
{
    FileHashResult r;
    r.ok = hashFile(localFilePath, r.sha1, r.sha512, r.fileSize, r.error);
    return r;
}

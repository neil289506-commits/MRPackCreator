#pragma once
#include <QString>
#include <qglobal.h>

// 使用 OpenSSL 的 libcrypto（EVP 介面）計算檔案的 SHA1 / SHA512，
// 並取得檔案大小，三者皆為 modrinth.index.json 中每個 file 項目必填欄位。
// 與 MCServer-Manager-17 相同，直接連結 third_party/openssl 底下的
// libcrypto.lib（編譯期）與執行期需要 libcrypto-*.dll，
// 因此不必另外呼叫 openssl.exe 命令列工具，計算完全在程式內部進行。
struct FileHashResult {
    bool    ok = false;
    QString sha1;
    QString sha512;
    qint64  fileSize = 0;
    QString error;
};

class HashCalculator
{
public:
    // 對單一本機檔案計算 sha1 / sha512 / fileSize。
    // 內部以串流方式讀取檔案（預設每次 1 MiB），避免大檔案一次性讀入記憶體。
    static FileHashResult calculate(const QString &localFilePath);
};

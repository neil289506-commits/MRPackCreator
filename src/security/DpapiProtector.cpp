
#include "DpapiProtector.h"
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

namespace { QString g_lastError; }

QString DpapiProtector::lastError() { return g_lastError; }

QByteArray DpapiProtector::protect(const QByteArray &plainData)
{
    if (plainData.isEmpty()) { g_lastError = "明文為空"; return {}; }

    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plainData.constData()));
    in.cbData = static_cast<DWORD>(plainData.size());

    DATA_BLOB out{};
    // 不傳 entropy（額外驗證字串）、不指定描述、CRYPTPROTECT_UI_FORBIDDEN
    // 確保絕不彈出系統對話框卡住背景流程，flags 不含
    // CRYPTPROTECT_LOCAL_MACHINE，代表綁定「目前使用者帳號」而非機器，
    // 這是刻意選擇：保護範圍縮小到帳號層級，比機器層級更嚴格。
    const BOOL ok = CryptProtectData(&in, L"MrpackMaker RSA Key",
                                      nullptr, nullptr, nullptr,
                                      CRYPTPROTECT_UI_FORBIDDEN, &out);
    if (!ok) {
        g_lastError = QString("CryptProtectData 失敗，錯誤碼 %1").arg(GetLastError());
        return {};
    }

    QByteArray result(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
}

QByteArray DpapiProtector::unprotect(const QByteArray &protectedData)
{
    if (protectedData.isEmpty()) { g_lastError = "密文為空"; return {}; }

    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(protectedData.constData()));
    in.cbData = static_cast<DWORD>(protectedData.size());

    DATA_BLOB out{};
    LPWSTR description = nullptr;

    const BOOL ok = CryptUnprotectData(&in, &description, nullptr, nullptr, nullptr,
                                        CRYPTPROTECT_UI_FORBIDDEN, &out);
    if (description) LocalFree(description);

    if (!ok) {
        // 這裡失敗是「正常且預期」的情況：檔案被複製到別台電腦、
        // 或用別的 Windows 帳號嘗試解密時，一定會走到這裡。
        g_lastError = QString("CryptUnprotectData 失敗，錯誤碼 %1"
                               "（若是換了電腦或 Windows 帳號，這是正常現象，"
                               "私鑰無法跨帳號/跨機器解密是刻意的安全設計）")
                          .arg(GetLastError());
        return {};
    }

    QByteArray result(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
}

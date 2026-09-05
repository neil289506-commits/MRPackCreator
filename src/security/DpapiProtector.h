#pragma once
#include <QByteArray>
#include <QString>

// 用 Windows DPAPI（Data Protection API）包裹 RSA 私鑰檔案本身，
// 這是這套加密機制真正的安全核心：
//
//   之前的做法：私鑰以明文 PEM 存在隨機路徑，只是「藏起來」，
//               任何能在這台電腦執行程式碼的人都能寫工具找到並讀取。
//
//   現在的做法：私鑰內容先用 CryptProtectData() 加密後才寫入磁碟。
//               DPAPI 的加密金鑰是從「目前登入的 Windows 使用者帳號」
//               的登入密碼雜湊衍生出來的，這代表：
//                 - 檔案被複製到別台電腦：解不開（換了機器的加密上下文）
//                 - 檔案被別的 Windows 帳號讀取：解不開（換了使用者）
//                 - 沒有這台電腦、這個帳號的登入權限：無法解密
//               這是微軟自己拿來保護瀏覽器已存密碼、憑證管理員私鑰的
//               同一套機制，不是我們自己發明的土砲方案。
//
// 使用 CRYPTPROTECT_LOCAL_MACHINE 旗標關閉（預設行為），也就是綁定
// 「使用者帳號」而非「機器」，這樣即使同一台電腦的其他 Windows 帳號
// 也讀不到，保護範圍更嚴格。
class DpapiProtector
{
public:
    // 加密任意 bytes，回傳加密後的 bytes（可直接寫入檔案）。
    // 失敗回傳空 QByteArray，透過 lastError() 查看原因。
    static QByteArray protect(const QByteArray &plainData);

    // 解密 protect() 產生的 bytes，回傳原始明文。
    // 若在別台電腦/別的帳號下呼叫，會確定失敗（這是設計預期行為，
    // 不是 bug）。失敗回傳空 QByteArray。
    static QByteArray unprotect(const QByteArray &protectedData);

    static QString lastError();
};

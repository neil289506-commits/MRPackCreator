#include "CurseForgeApi.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

CurseForgeApi::CurseForgeApi(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

// 統一組出符合官方文件規範的請求：
//   - Accept: application/json（官方文件明確要求，缺少時部分端點會拒絕或行為異常）
//   - x-api-key 用 QByteArray 而非直接 toUtf8()：先做基本驗證，避免使用者複製貼上
//     Key 時夾帶不可見字元（全形空白、換行等）導致送出無效 header 值造成 401，
//     卻只看到「網路錯誤」這種無意義訊息，難以排查。
static QNetworkRequest buildRequest(const QUrl &url, const QString &apiKey)
{
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("x-api-key", apiKey.trimmed().toUtf8());
    req.setHeader(QNetworkRequest::UserAgentHeader, "MCServerManager/1.0");
    return req;
}

// 解析 CurseForge API 失敗時回傳的錯誤內容。官方錯誤格式為：
//   { "errorCode": 401, "errorMessage": "Invalid API key" }
// 直接把這段訊息顯示給使用者，比 Qt 的通用 QNetworkReply::errorString()
// （通常只會是 "Error transferring ... server replied: Unauthorized"）有用得多。
static QString extractErrorMessage(QNetworkReply *reply, const QByteArray &body)
{
    const auto doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const auto obj = doc.object();
        if (obj.contains("errorMessage")) {
            return QString("CurseForge API 錯誤（%1）：%2")
                .arg(obj.value("errorCode").toInt())
                .arg(obj.value("errorMessage").toString());
        }
    }
    // 沒有解析到官方錯誤格式，退回顯示 HTTP 狀態碼 + Qt 的錯誤字串，
    // 至少比空白訊息有幫助。
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return QString("CurseForge 請求失敗（HTTP %1）：%2").arg(httpStatus).arg(reply->errorString());
}

void CurseForgeApi::searchMods(const QString &query,
                                const QString &gameVersion,
                                int modLoaderType,
                                int classId)
{
    if (!hasApiKey()) {
        emit errorOccurred("尚未設定 CurseForge API Key，請在模組搜尋視窗內設定。");
        return;
    }
    if (query.trimmed().isEmpty()) {
        emit errorOccurred("搜尋關鍵字不可為空");
        return;
    }

    // 官方文件：GET https://api.curseforge.com/v1/mods/search
    // 必要參數 gameId，其餘皆選填。QUrlQuery 會自動處理 percent-encoding
    // （包含中文、空白等特殊字元），不需要手動 QUrl::toPercentEncoding。
    QUrl url("https://api.curseforge.com/v1/mods/search");
    QUrlQuery q;
    q.addQueryItem("gameId", "432"); // 432 = Minecraft（官方固定值）
    q.addQueryItem("classId", QString::number(classId));
    q.addQueryItem("searchFilter", query.trimmed());
    q.addQueryItem("sortField", "2");       // 2 = Popularity（依熱門度排序，官方文件列舉值）
    q.addQueryItem("sortOrder", "desc");
    q.addQueryItem("pageSize", "20");
    if (!gameVersion.isEmpty())
        q.addQueryItem("gameVersion", gameVersion);
    if (modLoaderType > 0)
        q.addQueryItem("modLoaderType", QString::number(modLoaderType));
    url.setQuery(q);

    auto *reply = m_nam->get(buildRequest(url, m_apiKey));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply, body));
            return;
        }

        const auto doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit errorOccurred("CurseForge 回應格式異常（非合法 JSON）");
            return;
        }

        QVector<CurseForgeModSummary> results;
        for (const auto &v : doc.object().value("data").toArray()) {
            const auto obj = v.toObject();
            CurseForgeModSummary m;
            m.modId         = obj.value("id").toVariant().toLongLong();
            m.slug          = obj.value("slug").toString();
            m.name          = obj.value("name").toString();
            m.summary       = obj.value("summary").toString();
            m.iconUrl       = obj.value("logo").toObject().value("thumbnailUrl").toString();
            m.downloadCount = obj.value("downloadCount").toVariant().toLongLong();
            results.push_back(m);
        }
        emit searchFinished(results);
    });

    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError) {
        // 連線層級的錯誤（DNS 失敗、逾時、TLS 握手失敗等）在這裡就會觸發，
        // finished 訊號仍會後續發出，兩邊都處理避免漏掉錯誤情境。
        if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
            emit errorOccurred("CurseForge API Key 無效或已過期，請重新申請。");
        }
    });
}

void CurseForgeApi::fetchModFiles(qint64 modId, const QString &gameVersion, int modLoaderType)
{
    if (!hasApiKey()) {
        emit errorOccurred("尚未設定 CurseForge API Key，請在模組搜尋視窗內設定。");
        return;
    }
    if (modId <= 0) {
        emit errorOccurred("無效的 modId");
        return;
    }

    // 官方文件：GET /v1/mods/{modId}/files，gameVersion / modLoaderType
    // 皆為選填的 query 參數，用來過濾出「跟目前伺服器版本＋載入器相容」
    // 的檔案，不然清單會混進一堆裝不上去的舊版本檔案。
    QUrl url(QString("https://api.curseforge.com/v1/mods/%1/files").arg(modId));
    QUrlQuery q;
    if (!gameVersion.isEmpty())
        q.addQueryItem("gameVersion", gameVersion);
    if (modLoaderType > 0)
        q.addQueryItem("modLoaderType", QString::number(modLoaderType));
    if (!q.isEmpty())
        url.setQuery(q);

    auto *reply = m_nam->get(buildRequest(url, m_apiKey));
    connect(reply, &QNetworkReply::finished, this, [this, reply, modId]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply, body));
            return;
        }

        const auto doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit errorOccurred("CurseForge 回應格式異常（非合法 JSON）");
            return;
        }

        QVector<CurseForgeFileInfo> files;
        for (const auto &v : doc.object().value("data").toArray()) {
            const auto obj = v.toObject();
            CurseForgeFileInfo f;
            f.fileId      = obj.value("id").toVariant().toLongLong();
            f.displayName = obj.value("displayName").toString();
            f.fileDate    = obj.value("fileDate").toString();
            f.fileLength  = obj.value("fileLength").toVariant().toLongLong();
            files.push_back(f);
        }

        if (files.isEmpty()) {
            emit errorOccurred("這個模組沒有符合目前遊戲版本／載入器的檔案，請確認版本篩選條件。");
            return;
        }
        emit modFilesFinished(modId, files);
    });

    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError) {
        if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
            emit errorOccurred("CurseForge API Key 無效或已過期，請重新申請。");
        }
    });
}

void CurseForgeApi::fetchDownloadUrl(qint64 modId, qint64 fileId)
{
    if (!hasApiKey()) {
        emit errorOccurred("尚未設定 CurseForge API Key");
        return;
    }
    if (modId <= 0 || fileId <= 0) {
        emit errorOccurred("無效的 modId 或 fileId");
        return;
    }

    // 官方文件：GET /v1/mods/{modId}/files/{fileId}/download-url
    const QUrl url(QString("https://api.curseforge.com/v1/mods/%1/files/%2/download-url")
                   .arg(modId).arg(fileId));

    auto *reply = m_nam->get(buildRequest(url, m_apiKey));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(extractErrorMessage(reply, body));
            return;
        }

        const auto doc = QJsonDocument::fromJson(body);
        const QString downloadUrl = doc.object().value("data").toString();
        if (downloadUrl.isEmpty()) {
            emit errorOccurred("CurseForge 未回傳有效的下載連結（該檔案可能已被作者下架）");
            return;
        }
        emit downloadUrlFinished(downloadUrl);
    });
}

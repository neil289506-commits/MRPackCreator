#include "FileDownloader.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>
#include <QFileInfo>

FileDownloader::FileDownloader(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    // 依需求指定：下載完成的模組固定存放在 %TEMP%\Mrpack\Mods\，
    // 不論是使用者手動貼的直連網址、或透過模組搜尋（Modrinth/CurseForge）加入的項目，
    // 都走同一份下載邏輯、存到同一個資料夾，之後的雜湊計算步驟才能一視同仁處理。
    m_tempDir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                    .filePath("Mrpack/Mods");
    QDir().mkpath(m_tempDir);
}

void FileDownloader::download(const QString &url, const QString &suggestedFileName)
{
    QNetworkRequest req((QUrl(url)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setMaximumRedirectsAllowed(5);

    // 用 UUID 前綴避免不同來源同檔名互相覆蓋
    const QString safeName = suggestedFileName.isEmpty()
        ? QFileInfo(QUrl(url).path()).fileName()
        : suggestedFileName;
    const QString localPath = QDir(m_tempDir).filePath(
        QUuid::createUuid().toString(QUuid::WithoutBraces) + "_" + safeName);

    auto *file = new QFile(localPath, this);
    if (!file->open(QIODevice::WriteOnly)) {
        emit downloadError(url, tr("無法建立暫存檔：%1").arg(file->errorString()));
        file->deleteLater();
        return;
    }

    QNetworkReply *reply = m_net->get(req);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, url](qint64 recv, qint64 total) { emit downloadProgress(url, recv, total); });

    connect(reply, &QNetworkReply::readyRead, this,
            [reply, file]() { file->write(reply->readAll()); });

    connect(reply, &QNetworkReply::finished, this, [this, reply, file, url, localPath]() {
        file->write(reply->readAll());
        file->close();
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            file->remove();
            file->deleteLater();
            emit downloadError(url, reply->errorString());
            return;
        }
        file->deleteLater();
        emit downloadFinished(url, localPath);
    });
}

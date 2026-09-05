#pragma once
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

// 將直連網址下載到暫存目錄（%TEMP%/MrpackMaker/downloads），
// 供後續 HashCalculator 計算 sha1/sha512/fileSize 使用。
// 採單一 QNetworkAccessManager、逐一佇列下載（batch import 時依序處理），
// 方便在 UI 顯示目前是哪一個檔案、進度為何。
class FileDownloader : public QObject
{
    Q_OBJECT
public:
    explicit FileDownloader(QObject *parent = nullptr);

    // 開始下載單一網址；完成後 downloadFinished 會回傳暫存檔案的完整路徑。
    void download(const QString &url, const QString &suggestedFileName);

signals:
    void downloadProgress(const QString &url, qint64 received, qint64 total);
    void downloadFinished(const QString &url, const QString &localTempPath);
    void downloadError(const QString &url, const QString &error);

private:
    QNetworkAccessManager *m_net;
    QString m_tempDir;
};

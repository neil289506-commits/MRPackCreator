#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QStringList>
#include "../ModloaderInfo.h"

// 負責向各載入器官方 API 抓取「遊戲版本清單」與「載入器版本清單」。
// 端點與 MCServer-Manager-17/src/installer/VersionFetcher.cpp 保持一致，
// 只是這裡改用 QNetworkAccessManager 直接發請求，不再透過外部 Python 腳本。
class VersionFetcher : public QObject
{
    Q_OBJECT
public:
    explicit VersionFetcher(QObject *parent = nullptr);

    // 抓取 Mojang 官方的遊戲版本清單（僅 release，可選是否包含 snapshot）
    void fetchGameVersions(bool includeSnapshots = false);

    // 抓取指定載入器可用的版本清單。
    // mcVersion 對 Fabric/Quilt 可留空（其版本與 MC 版本無關聯，回傳全部載入器版本）；
    // Forge/NeoForge 則必須提供 mcVersion，因為其版本清單是綁定在特定 MC 版本底下。
    void fetchLoaderVersions(LoaderType type, const QString &mcVersion = QString());

signals:
    void progress(const QString &message);
    void gameVersionsReady(const QStringList &versions);
    void loaderVersionsReady(LoaderType type, const QStringList &versions);
    void fetchError(const QString &error);

private:
    void fetchFabricLike(LoaderType type, const QString &baseUrl);
    void fetchForge(const QString &mcVersion);
    void fetchNeoForge(const QString &mcVersion);

    QNetworkAccessManager *m_net;
};

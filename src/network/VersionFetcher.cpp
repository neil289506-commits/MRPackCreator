#include "VersionFetcher.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <algorithm>

VersionFetcher::VersionFetcher(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

void VersionFetcher::fetchGameVersions(bool includeSnapshots)
{
    emit progress(tr("正在抓取 Minecraft 遊戲版本清單..."));
    QNetworkRequest req(QUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, includeSnapshots]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(tr("抓取遊戲版本失敗：%1").arg(reply->errorString()));
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        QStringList out;
        for (const auto &v : doc.object().value("versions").toArray()) {
            const auto o = v.toObject();
            const QString type = o.value("type").toString();
            if (!includeSnapshots && type != "release")
                continue;
            out << o.value("id").toString();
        }
        emit gameVersionsReady(out);
    });
}

void VersionFetcher::fetchLoaderVersions(LoaderType type, const QString &mcVersion)
{
    switch (type) {
    case LoaderType::Fabric:
        fetchFabricLike(type, "https://meta.fabricmc.net/v2/versions/loader");
        break;
    case LoaderType::Quilt:
        fetchFabricLike(type, "https://meta.quiltmc.org/v3/versions/loader");
        break;
    case LoaderType::Forge:
        fetchForge(mcVersion);
        break;
    case LoaderType::NeoForge:
        fetchNeoForge(mcVersion);
        break;
    default:
        emit fetchError(tr("未知的載入器類型"));
        break;
    }
}

void VersionFetcher::fetchFabricLike(LoaderType type, const QString &baseUrl)
{
    emit progress(tr("正在抓取 %1 載入器版本清單...").arg(loaderDisplayName(type)));
    QNetworkRequest req((QUrl(baseUrl)));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, type]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(tr("抓取 %1 版本失敗：%2").arg(loaderDisplayName(type), reply->errorString()));
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        QStringList out;
        for (const auto &v : doc.array()) {
            const auto obj = v.toObject();
            QString ver;
            // Fabric API 回傳每筆含有 loader.version；
            // Quilt API 直接回傳 version（不在 loader 物件裡）。
            if (obj.contains("loader"))
                ver = obj.value("loader").toObject().value("version").toString();
            if (ver.isEmpty())
                ver = obj.value("version").toString();
            if (!ver.isEmpty() && !out.contains(ver))
                out << ver;
        }
        std::reverse(out.begin(), out.end()); // 讓新版本優先
        emit loaderVersionsReady(type, out);
    });
}

void VersionFetcher::fetchForge(const QString &mcVersion)
{
    if (mcVersion.isEmpty()) {
        emit fetchError(tr("Forge 版本清單需要先選擇遊戲版本"));
        return;
    }
    emit progress(tr("正在抓取 Forge 載入器版本清單..."));
    QNetworkRequest req((QUrl("https://maven.minecraftforge.net/net/minecraftforge/forge/maven-metadata.xml")));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, mcVersion]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(tr("抓取 Forge 版本失敗：%1").arg(reply->errorString()));
            return;
        }
        QXmlStreamReader xml(reply->readAll());
        QStringList out;
        const QString prefix = mcVersion + "-";
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QStringLiteral("version")) {
                const QString full = xml.readElementText(); // 例如 "1.20.1-47.2.0"
                if (full.startsWith(prefix)) {
                    // 去掉 "<mcVersion>-" 前綴，只留下真正的 loader 版本號
                    // 但部分早期版本後面還會再接 "-<mcVersion>" 後綴，一併去掉
                    QString loaderVer = full.mid(prefix.length());
                    const int extra = loaderVer.indexOf('-' + mcVersion);
                    if (extra >= 0)
                        loaderVer = loaderVer.left(extra);
                    out << loaderVer;
                }
            }
        }
        std::reverse(out.begin(), out.end()); // 讓最新版本排在前面
        emit loaderVersionsReady(LoaderType::Forge, out);
    });
}

void VersionFetcher::fetchNeoForge(const QString &mcVersion)
{
    if (mcVersion.isEmpty()) {
        emit fetchError(tr("NeoForge 版本清單需要先選擇遊戲版本"));
        return;
    }
    emit progress(tr("正在抓取 NeoForge 載入器版本清單..."));
    QNetworkRequest req((QUrl("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml")));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, mcVersion]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(tr("抓取 NeoForge 版本失敗：%1").arg(reply->errorString()));
            return;
        }
        // NeoForge 版本號格式為 "<mc次要版本>.<mc修訂版本>.<build>"，例如 MC 1.20.1 對應前綴 "20.1."
        QString shortMc = mcVersion;
        if (shortMc.startsWith("1."))
            shortMc = shortMc.mid(2);
        const QString prefix = shortMc + ".";

        QXmlStreamReader xml(reply->readAll());
        QStringList out;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QStringLiteral("version")) {
                const QString full = xml.readElementText();
                if (full.startsWith(prefix))
                    out << full;
            }
        }
        std::reverse(out.begin(), out.end());
        emit loaderVersionsReady(LoaderType::NeoForge, out);
    });
}

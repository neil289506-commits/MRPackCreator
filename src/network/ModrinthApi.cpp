#include "ModrinthApi.h"
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

ModrinthApi::ModrinthApi(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {}

void ModrinthApi::searchMods(const QString &query,
                              const QString &gameVersion,
                              const QString &loader,
                              const QString &projectType,
                              int limit)
{
    QUrl url(QString("%1/search").arg(kBaseUrl));
    QUrlQuery q;
    q.addQueryItem("query", query);
    q.addQueryItem("limit", QString::number(limit));

    // facets：版本 + loader + projectType，全部篩選
    QJsonArray facets;
    facets.append(QJsonArray{QString("project_type:%1").arg(projectType)});
    if (!gameVersion.isEmpty())
        facets.append(QJsonArray{QString("versions:%1").arg(gameVersion)});
    if (!loader.isEmpty() && loader != "vanilla" && loader != "paper" && loader != "purpur")
        facets.append(QJsonArray{QString("categories:%1").arg(loader.toLower())});

    q.addQueryItem("facets", QJsonDocument(facets).toJson(QJsonDocument::Compact));
    url.setQuery(q);

    auto *reply = m_nam->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString()); return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        QVector<ModrinthModSummary> results;
        for (const auto &v : doc.object().value("hits").toArray()) {
            const auto obj = v.toObject();
            ModrinthModSummary m;
            m.projectId   = obj.value("project_id").toString();
            m.slug        = obj.value("slug").toString();
            m.title       = obj.value("title").toString();
            m.description = obj.value("description").toString();
            m.iconUrl     = obj.value("icon_url").toString();
            m.downloads   = obj.value("downloads").toInt();
            results.push_back(m);
        }
        emit searchFinished(results);
    });
}

void ModrinthApi::fetchVersionFiles(const QString &projectIdOrSlug,
                                     const QString &gameVersion,
                                     const QString &loader)
{
    QUrl url(QString("%1/project/%2/version").arg(kBaseUrl, projectIdOrSlug));
    QUrlQuery q;
    if (!gameVersion.isEmpty())
        q.addQueryItem("game_versions", QString("[\"%1\"]").arg(gameVersion));
    if (!loader.isEmpty() && loader != "vanilla" && loader != "paper" && loader != "purpur")
        q.addQueryItem("loaders", QString("[\"%1\"]").arg(loader.toLower()));
    url.setQuery(q);

    auto *reply = m_nam->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString()); return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        QVector<ModrinthFileInfo> files;
        for (const auto &versionVal : doc.array()) {
            for (const auto &fileVal : versionVal.toObject().value("files").toArray()) {
                const auto obj = fileVal.toObject();
                ModrinthFileInfo f;
                f.filename    = obj.value("filename").toString();
                f.downloadUrl = obj.value("url").toString();
                f.sha1        = obj.value("hashes").toObject().value("sha1").toString();
                f.isPrimary   = obj.value("primary").toBool();
                files.push_back(f);
            }
        }
        emit versionFilesFinished(files);
    });
}

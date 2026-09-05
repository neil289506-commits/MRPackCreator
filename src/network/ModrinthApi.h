#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QVector>

struct ModrinthModSummary {
    QString projectId;
    QString slug;
    QString title;
    QString description;
    QString iconUrl;
    int     downloads = 0;
};

struct ModrinthFileInfo {
    QString filename;
    QString downloadUrl;
    QString sha1;
    bool    isPrimary = false;
};

class ModrinthApi : public QObject
{
    Q_OBJECT
public:
    explicit ModrinthApi(QObject *parent = nullptr);

    // projectType: "mod" / "resourcepack" / "datapack"
    void searchMods(const QString &query,
                    const QString &gameVersion,
                    const QString &loader,
                    const QString &projectType = "mod",
                    int limit = 20);

    void fetchVersionFiles(const QString &projectIdOrSlug,
                            const QString &gameVersion,
                            const QString &loader);

signals:
    void searchFinished(const QVector<ModrinthModSummary> &results);
    void versionFilesFinished(const QVector<ModrinthFileInfo> &files);
    void errorOccurred(const QString &message);

private:
    QNetworkAccessManager *m_nam;
    static constexpr const char *kBaseUrl = "https://api.modrinth.com/v2";
};

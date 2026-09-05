#include "KeyPathGenerator.h"
#include <QStandardPaths>
#include <QRandomGenerator>
#include <QDir>

QString KeyPathGenerator::randomSegment(int length)
{
    static const QString charset = "abcdefghijklmnopqrstuvwxyz0123456789";
    QString result;
    result.reserve(length);
    for (int i = 0; i < length; ++i) {
        const int idx = QRandomGenerator::global()->bounded(charset.length());
        result.append(charset.at(idx));
    }
    return result;
}

QString KeyPathGenerator::pickRandomBaseDir()
{
    const QStringList candidates = {
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation),
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation),
        QStandardPaths::writableLocation(QStandardPaths::TempLocation),
    };
    QStringList valid;
    for (const QString &c : candidates) if (!c.isEmpty()) valid << c;
    if (valid.isEmpty())
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const int idx = QRandomGenerator::global()->bounded(valid.size());
    return valid.at(idx);
}

void KeyPathGenerator::generatePaths(QString &outPrivateKeyPath, QString &outPublicKeyPath)
{
    const QString base = pickRandomBaseDir();
    const QString dir = QDir(base).filePath(
        randomSegment(8) + "/" + randomSegment(10) + "/" + randomSegment(6));
    QDir().mkpath(dir);

    const QString privName = randomSegment(12) + ".dat";
    const QString pubName  = randomSegment(12) + ".cache";
    outPrivateKeyPath = QDir(dir).filePath(privName);
    outPublicKeyPath  = QDir(dir).filePath(pubName);
}

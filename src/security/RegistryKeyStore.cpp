#include "RegistryKeyStore.h"
#include <QSettings>
#include <QRandomGenerator>

QString RegistryKeyStore::registryRootPath()
{
    // 對應 HKEY_CURRENT_USER\Software\MrpackMaker
    return "HKEY_CURRENT_USER\\Software\\MrpackMaker";
}

QString RegistryKeyStore::generateRandomValueName()
{
    const quint64 randomPart = QRandomGenerator::global()->generate64();
    return QString("k_%1").arg(randomPart, 16, 16, QChar('0'));
}

bool RegistryKeyStore::tryLoadKeyPaths(QString &outPrivateKeyPath, QString &outPublicKeyPath)
{
    QSettings reg(registryRootPath(), QSettings::NativeFormat);

    const QString privValueNameKey = reg.value("_a").toString();
    const QString pubValueNameKey  = reg.value("_b").toString();
    if (privValueNameKey.isEmpty() || pubValueNameKey.isEmpty())
        return false;

    outPrivateKeyPath = reg.value(privValueNameKey).toString();
    outPublicKeyPath  = reg.value(pubValueNameKey).toString();
    return !outPrivateKeyPath.isEmpty() && !outPublicKeyPath.isEmpty();
}

bool RegistryKeyStore::saveKeyPaths(const QString &privateKeyPath, const QString &publicKeyPath)
{
    QSettings reg(registryRootPath(), QSettings::NativeFormat);

    const QString privValueName = generateRandomValueName();
    QString pubValueName;
    do { pubValueName = generateRandomValueName(); } while (pubValueName == privValueName);

    reg.setValue("_a", privValueName);
    reg.setValue("_b", pubValueName);
    reg.setValue(privValueName, privateKeyPath);
    reg.setValue(pubValueName,  publicKeyPath);
    reg.sync();

    return reg.status() == QSettings::NoError;
}

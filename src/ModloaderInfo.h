#pragma once
#include <QString>

// Modrinth mrpack 規格的 dependencies 目前只認可這幾種載入器 key，
// 其餘（如 OptiFine）並非官方支援的 mrpack 載入器，不列入此工具。
enum class LoaderType {
    Fabric,
    Forge,
    NeoForge,
    Quilt,
    Unknown
};

// 對應 modrinth.index.json -> dependencies 裡使用的 key 名稱
inline QString loaderDependencyKey(LoaderType t)
{
    switch (t) {
    case LoaderType::Fabric:   return QStringLiteral("fabric-loader");
    case LoaderType::Forge:    return QStringLiteral("forge");
    case LoaderType::NeoForge: return QStringLiteral("neoforge");
    case LoaderType::Quilt:    return QStringLiteral("quilt-loader");
    default:                   return QString();
    }
}

inline QString loaderDisplayName(LoaderType t)
{
    switch (t) {
    case LoaderType::Fabric:   return QStringLiteral("Fabric");
    case LoaderType::Forge:    return QStringLiteral("Forge");
    case LoaderType::NeoForge: return QStringLiteral("NeoForge");
    case LoaderType::Quilt:    return QStringLiteral("Quilt");
    default:                   return QStringLiteral("Unknown");
    }
}

inline LoaderType loaderFromDisplayName(const QString &name)
{
    if (name == QStringLiteral("Fabric"))   return LoaderType::Fabric;
    if (name == QStringLiteral("Forge"))    return LoaderType::Forge;
    if (name == QStringLiteral("NeoForge")) return LoaderType::NeoForge;
    if (name == QStringLiteral("Quilt"))    return LoaderType::Quilt;
    return LoaderType::Unknown;
}

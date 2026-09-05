#include "MrpackBuilder.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDirIterator>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QVector>

#include <JlCompress.h>
#include <quazip.h>
#include <quazipfile.h>

MrpackBuilder::MrpackBuilder(QObject *parent) : QObject(parent) {}

QByteArray MrpackBuilder::buildIndexJson(const ModpackData &data) const
{
    QJsonObject root;
    root["formatVersion"] = 1;
    root["game"] = "minecraft";
    root["versionId"] = data.basic.versionId;
    root["name"] = data.basic.name;
    if (!data.basic.summary.isEmpty())
        root["summary"] = data.basic.summary;

    QJsonObject deps;
    deps["minecraft"] = data.basic.mcVersion;
    const QString loaderKey = loaderDependencyKey(data.basic.loaderType);
    if (!loaderKey.isEmpty())
        deps[loaderKey] = data.basic.loaderVersion;
    root["dependencies"] = deps;

    QJsonArray files;
    for (const auto &f : data.files) {
        QJsonObject fo;
        fo["path"] = f.path;

        QJsonObject hashes;
        hashes["sha1"] = f.sha1;
        hashes["sha512"] = f.sha512;
        fo["hashes"] = hashes;

        QJsonObject env;
        env["client"] = envToString(f.clientEnv);
        env["server"] = envToString(f.serverEnv);
        fo["env"] = env;

        QJsonArray downloads;
        for (const auto &u : f.downloads)
            downloads.append(u);
        fo["downloads"] = downloads;

        fo["fileSize"] = f.fileSize;
        files.append(fo);
    }
    root["files"] = files;

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

int MrpackBuilder::countPath(const QString &absPath) const
{
    const QFileInfo fi(absPath);
    if (!fi.isDir())
        return 1;
    int count = 0;
    QDirIterator it(absPath, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

int MrpackBuilder::countSelection(const OverrideSelection &sel) const
{
    if (sel.instanceDir.isEmpty() || sel.selectedPaths.isEmpty())
        return 0;
    int total = 0;
    for (const QString &rel : sel.selectedPaths)
        total += countPath(QDir(sel.instanceDir).filePath(rel));
    return total;
}

bool MrpackBuilder::addPathToZip(QuaZip &zip, const QString &absPath, const QString &zipPath,
                                  int &doneCounter, int totalCounter, QString &errorOut)
{
    const QFileInfo fi(absPath);
    if (fi.isDir()) {
        QDirIterator it(absPath, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString childAbs = it.next();
            const QString relFromHere = QDir(absPath).relativeFilePath(childAbs);
            if (!addPathToZip(zip, childAbs, zipPath + "/" + relFromHere, doneCounter, totalCounter, errorOut))
                return false;
        }
        return true;
    }

    QFile inFile(absPath);
    if (!inFile.open(QIODevice::ReadOnly)) {
        errorOut = tr("無法讀取覆蓋檔案：%1").arg(absPath);
        return false;
    }

    QuaZipFile outFile(&zip);
    QuaZipNewInfo info(zipPath, absPath);
    if (!outFile.open(QIODevice::WriteOnly, info, nullptr, 0, Z_DEFLATED)) {
        errorOut = tr("無法寫入 zip 項目：%1").arg(zipPath);
        return false;
    }
    outFile.write(inFile.readAll());
    outFile.close();
    if (outFile.getZipError() != UNZ_OK) {
        errorOut = tr("寫入 zip 項目時發生錯誤：%1（code %2）").arg(zipPath).arg(outFile.getZipError());
        return false;
    }

    ++doneCounter;
    emit stageProgress(doneCounter, totalCounter);
    emit progress(tr("已加入：%1").arg(zipPath));
    return true;
}

bool MrpackBuilder::addSelectionToZip(QuaZip &zip, const OverrideSelection &sel, const QString &zipPrefix,
                                       int &doneCounter, int totalCounter, QString &errorOut)
{
    if (sel.instanceDir.isEmpty() || sel.selectedPaths.isEmpty())
        return true;

    if (!QDir(sel.instanceDir).exists()) {
        errorOut = tr("實例資料夾不存在：%1").arg(sel.instanceDir);
        return false;
    }

    for (const QString &rel : sel.selectedPaths) {
        const QString absPath = QDir(sel.instanceDir).filePath(rel);
        if (!QFileInfo::exists(absPath)) {
            errorOut = tr("找不到選取的項目：%1").arg(absPath);
            return false;
        }
        if (!addPathToZip(zip, absPath, zipPrefix + "/" + rel, doneCounter, totalCounter, errorOut))
            return false;
    }
    return true;
}

bool MrpackBuilder::exportMrpack(const ModpackData &data, const QString &outputPath, QString &errorOut)
{
    // 匯出前檢查：每個檔案項目都必須先算好雜湊/大小，且至少要有一個 downloads URL
    for (const auto &f : data.files) {
        if (!f.hashComputed || f.sha1.isEmpty() || f.sha512.isEmpty()) {
            errorOut = tr("檔案「%1」尚未計算雜湊值，請先在檔案清單頁執行計算。").arg(f.path);
            return false;
        }
        if (f.downloads.isEmpty()) {
            errorOut = tr("檔案「%1」缺少下載網址，mrpack 規格要求每個檔案至少要有一個 downloads URL。"
                          "本機匯入的模組需自行提供一個可公開存取的下載連結（例如 CDN、Modrinth 上對應版本的直連）。")
                          .arg(f.path);
            return false;
        }
    }

    QuaZip zip(outputPath);
    if (!zip.open(QuaZip::mdCreate)) {
        errorOut = tr("無法建立輸出檔案：%1").arg(outputPath);
        return false;
    }

    // 建立階段清單：只列出實際有內容的階段，讓「總進度」準確反映真正要做的事，
    // 而不是永遠固定 4 個階段但有些階段其實是空的。
    struct StageDef { QString name; const OverrideSelection *sel; bool isIndex; };
    QVector<StageDef> stages;
    stages.append({tr("寫入 modrinth.index.json"), nullptr, true});
    if (!data.overrides.overrides.instanceDir.isEmpty() && !data.overrides.overrides.selectedPaths.isEmpty())
        stages.append({tr("加入 overrides"), &data.overrides.overrides, false});
    if (!data.overrides.clientOverrides.instanceDir.isEmpty() && !data.overrides.clientOverrides.selectedPaths.isEmpty())
        stages.append({tr("加入 client-overrides"), &data.overrides.clientOverrides, false});
    if (!data.overrides.serverOverrides.instanceDir.isEmpty() && !data.overrides.serverOverrides.selectedPaths.isEmpty())
        stages.append({tr("加入 server-overrides"), &data.overrides.serverOverrides, false});

    const int totalStages = stages.size();
    for (int i = 0; i < stages.size(); ++i) {
        const auto &st = stages[i];
        emit stageChanged(i + 1, totalStages, st.name);

        if (st.isIndex) {
            emit stageProgress(0, 1);
            const QByteArray json = buildIndexJson(data);
            QuaZipFile indexFile(&zip);
            QuaZipNewInfo info("modrinth.index.json");
            if (!indexFile.open(QIODevice::WriteOnly, info, nullptr, 0, Z_DEFLATED)) {
                errorOut = tr("無法寫入 modrinth.index.json");
                zip.close();
                return false;
            }
            indexFile.write(json);
            indexFile.close();
            emit stageProgress(1, 1);
            emit progress(tr("已寫入 modrinth.index.json"));
        } else {
            const int total = countSelection(*st.sel);
            int done = 0;
            emit stageProgress(done, total);
            const QString prefix = (st.sel == &data.overrides.overrides) ? "overrides"
                                  : (st.sel == &data.overrides.clientOverrides) ? "client-overrides"
                                                                                 : "server-overrides";
            emit progress(tr("正在加入 %1 ...").arg(prefix));
            if (!addSelectionToZip(zip, *st.sel, prefix, done, total, errorOut)) {
                zip.close();
                return false;
            }
        }
    }

    zip.close();
    if (zip.getZipError() != UNZ_OK) {
        errorOut = tr("關閉 zip 檔時發生錯誤（code %1）").arg(zip.getZipError());
        return false;
    }

    emit stageChanged(totalStages, totalStages, tr("完成"));
    emit progress(tr("匯出完成：%1").arg(outputPath));
    return true;
}

#include "ModSearchDialog.h"
#include "../security/CurseForgeSecureStorage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrl>

namespace {
constexpr int RoleModrinthProjectId = Qt::UserRole;
constexpr int RoleCurseForgeModId = Qt::UserRole;

constexpr int RoleFileUrl = Qt::UserRole;
constexpr int RoleFileName = Qt::UserRole + 1;
constexpr int RoleCfFileId = Qt::UserRole + 2;
}

ModSearchDialog::ModSearchDialog(const QString &mcVersion, LoaderType loaderType, QWidget *parent)
    : QDialog(parent)
    , m_mcVersion(mcVersion)
    , m_loaderType(loaderType)
    , m_modrinth(new ModrinthApi(this))
    , m_curseforge(new CurseForgeApi(this))
{
    setWindowTitle(tr("搜尋並加入模組"));
    resize(760, 560);

    m_providerCombo = new QComboBox(this);
    m_providerCombo->addItem("Modrinth");
    m_providerCombo->addItem("CurseForge");

    m_queryEdit = new QLineEdit(this);
    m_queryEdit->setPlaceholderText(tr("輸入模組名稱關鍵字..."));
    connect(m_queryEdit, &QLineEdit::returnPressed, this, &ModSearchDialog::onSearch);

    m_searchBtn = new QPushButton(tr("搜尋"), this);
    connect(m_searchBtn, &QPushButton::clicked, this, &ModSearchDialog::onSearch);

    m_apiKeyBtn = new QPushButton(tr("設定 CurseForge API Key..."), this);
    connect(m_apiKeyBtn, &QPushButton::clicked, this, &ModSearchDialog::onSetCurseForgeApiKey);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(new QLabel(tr("來源："), this));
    topRow->addWidget(m_providerCombo);
    topRow->addWidget(m_queryEdit, 1);
    topRow->addWidget(m_searchBtn);
    topRow->addWidget(m_apiKeyBtn);

    m_resultList = new QListWidget(this);
    connect(m_resultList, &QListWidget::itemClicked, this, &ModSearchDialog::onResultSelected);

    m_fileList = new QListWidget(this);
    connect(m_fileList, &QListWidget::itemClicked, this, &ModSearchDialog::onFileSelected);

    auto *listsRow = new QHBoxLayout;
    auto *resultCol = new QVBoxLayout;
    resultCol->addWidget(new QLabel(tr("搜尋結果："), this));
    resultCol->addWidget(m_resultList);
    auto *fileCol = new QVBoxLayout;
    fileCol->addWidget(new QLabel(tr("相容檔案（依目前 %1 / %2 過濾）：")
                                   .arg(mcVersion, loaderDisplayName(loaderType)), this));
    fileCol->addWidget(m_fileList);
    listsRow->addLayout(resultCol, 1);
    listsRow->addLayout(fileCol, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);

    m_addBtn = new QPushButton(tr("加入清單"), this);
    m_addBtn->setObjectName("primaryButton");
    m_addBtn->setEnabled(false);
    connect(m_addBtn, &QPushButton::clicked, this, &ModSearchDialog::onAddChosenFile);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addWidget(m_statusLabel, 1);
    bottomRow->addWidget(m_addBtn);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addLayout(listsRow, 1);
    layout->addLayout(bottomRow);

    connect(m_modrinth, &ModrinthApi::searchFinished, this, [this](const QVector<ModrinthModSummary> &results) {
        m_resultList->clear();
        for (const auto &m : results) {
            auto *item = new QListWidgetItem(tr("%1  (%2 次下載)").arg(m.title).arg(m.downloads));
            item->setData(RoleModrinthProjectId, m.projectId);
            m_resultList->addItem(item);
        }
        log(tr("Modrinth 搜尋完成，共 %1 筆結果").arg(results.size()));
    });
    connect(m_modrinth, &ModrinthApi::versionFilesFinished, this, [this](const QVector<ModrinthFileInfo> &files) {
        m_fileList->clear();
        for (const auto &f : files) {
            auto *item = new QListWidgetItem(f.filename + (f.isPrimary ? tr("（主要檔案）") : QString()));
            item->setData(RoleFileUrl, f.downloadUrl);
            item->setData(RoleFileName, f.filename);
            m_fileList->addItem(item);
        }
        log(tr("已取得 %1 個相容檔案").arg(files.size()));
    });
    connect(m_modrinth, &ModrinthApi::errorOccurred, this, [this](const QString &e) { log(tr("Modrinth 錯誤：%1").arg(e)); });

    connect(m_curseforge, &CurseForgeApi::searchFinished, this, [this](const QVector<CurseForgeModSummary> &results) {
        m_resultList->clear();
        for (const auto &m : results) {
            auto *item = new QListWidgetItem(tr("%1  (%2 次下載)").arg(m.name).arg(m.downloadCount));
            item->setData(RoleCurseForgeModId, m.modId);
            m_resultList->addItem(item);
        }
        log(tr("CurseForge 搜尋完成，共 %1 筆結果").arg(results.size()));
    });
    connect(m_curseforge, &CurseForgeApi::modFilesFinished, this, [this](qint64, const QVector<CurseForgeFileInfo> &files) {
        m_fileList->clear();
        for (const auto &f : files) {
            auto *item = new QListWidgetItem(tr("%1（%2 KB）").arg(f.displayName).arg(f.fileLength / 1024));
            item->setData(RoleCfFileId, f.fileId);
            item->setData(RoleFileName, f.displayName);
            m_fileList->addItem(item);
        }
        log(tr("已取得 %1 個相容檔案").arg(files.size()));
    });
    connect(m_curseforge, &CurseForgeApi::downloadUrlFinished, this, [this](const QString &url) {
        ModFileEntry e;
        e.path = "mods/" + QFileInfo(QUrl(url).path()).fileName();
        e.downloads = {url};
        emit modsChosen({e});
        log(tr("已加入：%1").arg(e.path));
    });
    connect(m_curseforge, &CurseForgeApi::errorOccurred, this, [this](const QString &e) { log(tr("CurseForge 錯誤：%1").arg(e)); });

    // 開啟對話框時嘗試先讀取已保存的 CurseForge API Key（RSA 混合加密 + DPAPI 保護私鑰）
    if (CurseForgeSecureStorage::ensureKeyPairExists()) {
        const QString saved = CurseForgeSecureStorage::loadApiKey();
        if (!saved.isEmpty())
            m_curseforge->setApiKey(saved);
    }
}

void ModSearchDialog::log(const QString &msg)
{
    m_statusLabel->setText(msg);
}

int ModSearchDialog::curseForgeLoaderTypeId() const
{
    // CurseForge 官方 modLoaderType 列舉值：1=Forge, 4=Fabric, 5=Quilt, 6=NeoForge
    switch (m_loaderType) {
    case LoaderType::Forge:    return 1;
    case LoaderType::Fabric:   return 4;
    case LoaderType::Quilt:    return 5;
    case LoaderType::NeoForge: return 6;
    default:                   return 0;
    }
}

void ModSearchDialog::onSearch()
{
    const QString query = m_queryEdit->text().trimmed();
    if (query.isEmpty()) {
        log(tr("請輸入搜尋關鍵字"));
        return;
    }
    m_fileList->clear();
    m_addBtn->setEnabled(false);

    if (m_providerCombo->currentText() == "Modrinth") {
        log(tr("正在搜尋 Modrinth..."));
        m_modrinth->searchMods(query, m_mcVersion, loaderDisplayName(m_loaderType).toLower());
    } else {
        if (!m_curseforge->hasApiKey()) {
            log(tr("尚未設定 CurseForge API Key，請先點右上角按鈕設定"));
            return;
        }
        log(tr("正在搜尋 CurseForge..."));
        m_curseforge->searchMods(query, m_mcVersion, curseForgeLoaderTypeId());
    }
}

void ModSearchDialog::onResultSelected(QListWidgetItem *item)
{
    m_fileList->clear();
    m_addBtn->setEnabled(false);

    if (m_providerCombo->currentText() == "Modrinth") {
        const QString projectId = item->data(RoleModrinthProjectId).toString();
        log(tr("正在取得檔案清單..."));
        m_modrinth->fetchVersionFiles(projectId, m_mcVersion, loaderDisplayName(m_loaderType).toLower());
    } else {
        const qint64 modId = item->data(RoleCurseForgeModId).toLongLong();
        log(tr("正在取得檔案清單..."));
        m_curseforge->fetchModFiles(modId, m_mcVersion, curseForgeLoaderTypeId());
    }
}

void ModSearchDialog::onFileSelected(QListWidgetItem *item)
{
    m_pendingIsCurseForge = (m_providerCombo->currentText() != "Modrinth");
    m_pendingPath = "mods/" + item->data(RoleFileName).toString();

    if (m_pendingIsCurseForge) {
        m_pendingCfFileId = item->data(RoleCfFileId).toLongLong();
        // modId 取自目前選取的搜尋結果（result list 目前選取項）
        auto *resultItem = m_resultList->currentItem();
        m_pendingCfModId = resultItem ? resultItem->data(RoleCurseForgeModId).toLongLong() : 0;
    } else {
        m_pendingUrl = item->data(RoleFileUrl).toString();
    }
    m_addBtn->setEnabled(true);
}

void ModSearchDialog::onAddChosenFile()
{
    if (m_pendingIsCurseForge) {
        if (m_pendingCfModId <= 0 || m_pendingCfFileId <= 0) {
            log(tr("選取的檔案資訊不完整"));
            return;
        }
        log(tr("正在取得 CurseForge 下載連結..."));
        m_curseforge->fetchDownloadUrl(m_pendingCfModId, m_pendingCfFileId);
    } else {
        if (m_pendingUrl.isEmpty()) {
            log(tr("選取的檔案缺少下載網址"));
            return;
        }
        ModFileEntry e;
        e.path = m_pendingPath;
        e.downloads = {m_pendingUrl};
        emit modsChosen({e});
        log(tr("已加入：%1").arg(e.path));
    }
}

void ModSearchDialog::onSetCurseForgeApiKey()
{
    if (!CurseForgeSecureStorage::ensureKeyPairExists()) {
        QMessageBox::warning(this, tr("金鑰初始化失敗"), CurseForgeSecureStorage::lastError());
        return;
    }
    const QString current = CurseForgeSecureStorage::loadApiKey();
    bool ok = false;
    const QString key = QInputDialog::getText(this, tr("設定 CurseForge API Key"),
                                                tr("請輸入 CurseForge API Key（會用 RSA 混合加密＋DPAPI 保護私鑰後存在本機）："),
                                                QLineEdit::Password, current, &ok);
    if (!ok)
        return;

    if (!CurseForgeSecureStorage::saveApiKey(key)) {
        QMessageBox::warning(this, tr("儲存失敗"), CurseForgeSecureStorage::lastError());
        return;
    }
    m_curseforge->setApiKey(key.trimmed());
    log(key.trimmed().isEmpty() ? tr("已清除 CurseForge API Key") : tr("已儲存 CurseForge API Key"));
}

#include "FilesPage.h"
#include "../core/FileDownloader.h"
#include "../core/HashCalculator.h"
#include "ModSearchDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QUrl>
#include <QDateTime>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

namespace {
enum Column { ColPath = 0, ColUrl, ColClient, ColServer, ColSha1, ColSha512, ColSize, ColStatus, ColCount };

EnvSupport comboToEnv(const QComboBox *c)
{
    switch (c->currentIndex()) {
    case 0: return EnvSupport::Required;
    case 1: return EnvSupport::Optional;
    default: return EnvSupport::Unsupported;
    }
}

QComboBox *makeEnvCombo(QWidget *parent)
{
    auto *c = new QComboBox(parent);
    c->addItem(QObject::tr("必要 (required)"));
    c->addItem(QObject::tr("可選 (optional)"));
    c->addItem(QObject::tr("不支援 (unsupported)"));
    return c;
}

int envToComboIndex(EnvSupport e)
{
    switch (e) {
    case EnvSupport::Required: return 0;
    case EnvSupport::Optional: return 1;
    default: return 2;
    }
}
} // namespace

FilesPage::FilesPage(ModpackData *data, QWidget *parent)
    : QWizardPage(parent)
    , m_data(data)
    , m_downloader(new FileDownloader(this))
{
    setTitle(tr("匯入模組檔案"));
    setSubTitle(tr("支援直連網址（可批次貼多行）、本機檔案（可複選），或直接搜尋 Modrinth / CurseForge 加入，"
                    "匯入後請按「計算雜湊值與大小」，會自動下載/讀取檔案並算出 sha1、sha512 與檔案大小"
                    "（下載暫存於 %TEMP%\\Mrpack\\Mods\\）。"));

    m_table = new QTableWidget(0, ColCount, this);
    m_table->setHorizontalHeaderLabels({
        tr("路徑 (相對於 .minecraft，例如 mods/xxx.jar)"),
        tr("下載網址"),
        tr("Client"),
        tr("Server"),
        tr("SHA1"),
        tr("SHA512"),
        tr("大小 (bytes)"),
        tr("狀態"),
    });
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColPath, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColUrl, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    auto *btnAddUrls = new QPushButton(tr("新增直連網址（可批次多行）"), this);
    auto *btnAddLocal = new QPushButton(tr("新增本機檔案（可複選）"), this);
    auto *btnAddSearch = new QPushButton(tr("搜尋 Modrinth / CurseForge..."), this);
    auto *btnRemove = new QPushButton(tr("移除選取列"), this);
    auto *btnCompute = new QPushButton(tr("計算雜湊值與大小"), this);
    btnCompute->setObjectName("primaryButton");

    connect(btnAddUrls, &QPushButton::clicked, this, &FilesPage::onAddUrls);
    connect(btnAddLocal, &QPushButton::clicked, this, &FilesPage::onAddLocalFiles);
    connect(btnAddSearch, &QPushButton::clicked, this, &FilesPage::onAddFromSearch);
    connect(btnRemove, &QPushButton::clicked, this, &FilesPage::onRemoveSelected);
    connect(btnCompute, &QPushButton::clicked, this, &FilesPage::onComputeAll);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(btnAddUrls);
    btnRow->addWidget(btnAddLocal);
    btnRow->addWidget(btnAddSearch);
    btnRow->addWidget(btnRemove);
    btnRow->addStretch();
    btnRow->addWidget(btnCompute);

    // 三段式進度：總進度（佇列 X/N）、階段進度（目前這筆檔案的下載百分比）、目前階段文字說明
    m_stageLabel = new QLabel(tr("尚未開始"), this);

    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setRange(0, 1);
    m_overallProgress->setValue(0);
    m_overallProgress->setFormat(tr("總進度：%v / %m"));

    m_stageProgress = new QProgressBar(this);
    m_stageProgress->setRange(0, 1);
    m_stageProgress->setValue(0);
    m_stageProgress->setFormat(tr("階段進度：%p%"));

    m_console = new QPlainTextEdit(this);
    m_console->setObjectName("consoleOutput");
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(1000);
    m_console->setFixedHeight(110);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(btnRow);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_stageLabel);
    layout->addWidget(m_overallProgress);
    layout->addWidget(m_stageProgress);
    layout->addWidget(new QLabel(tr("處理紀錄："), this));
    layout->addWidget(m_console);

    // 佇列採「循序處理」，同一時間只會有一筆下載在進行，所以這裡只需要一組固定連線，
    // 用 m_pendingRow 記錄目前是哪一列，不必每次處理都重新 connect（避免重複連線疊加）。
    connect(m_downloader, &FileDownloader::downloadProgress, this,
            [this](const QString &, qint64 received, qint64 total) {
        if (total > 0) {
            m_stageProgress->setRange(0, 100);
            m_stageProgress->setValue(static_cast<int>(received * 100 / total));
        }
    });

    connect(m_downloader, &FileDownloader::downloadFinished, this,
            [this](const QString &, const QString &tempPath) {
        if (m_pendingRow < 0)
            return;
        const int row = m_pendingRow;
        m_pendingRow = -1;
        m_stageLabel->setText(tr("正在計算雜湊值：%1").arg(m_table->item(row, ColPath)->text()));
        m_stageProgress->setRange(0, 0); // 雜湊計算沒有逐步進度，改用忙碌指示
        auto *watcher = new QFutureWatcher<FileHashResult>(this);
        connect(watcher, &QFutureWatcher<FileHashResult>::finished, this, [this, watcher, row]() {
            applyHashResult(row, watcher->result());
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run([tempPath]() { return HashCalculator::calculate(tempPath); }));
    });

    connect(m_downloader, &FileDownloader::downloadError, this, [this](const QString &url, const QString &err) {
        log(tr("下載失敗 [%1]：%2").arg(url, err));
        if (m_pendingRow < 0)
            return;
        const int row = m_pendingRow;
        m_pendingRow = -1;
        m_table->item(row, ColStatus)->setText(tr("下載失敗：%1").arg(err));
        processNextInQueue();
    });
}

void FilesPage::log(const QString &msg)
{
    m_console->appendPlainText(QDateTime::currentDateTime().toString("[hh:mm:ss] ") + msg);
}

QString FilesPage::guessPathFromName(const QString &fileName) const
{
    return "mods/" + fileName;
}

int FilesPage::addRow(const ModFileEntry &entry)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    setRowFromEntry(row, entry);
    return row;
}

void FilesPage::setRowFromEntry(int row, const ModFileEntry &entry)
{
    auto *pathItem = new QTableWidgetItem(entry.path);
    pathItem->setData(Qt::UserRole, entry.localTempPath);
    pathItem->setData(Qt::UserRole + 1, entry.isLocalImport);
    m_table->setItem(row, ColPath, pathItem);

    m_table->setItem(row, ColUrl, new QTableWidgetItem(entry.downloads.isEmpty() ? QString() : entry.downloads.first()));

    auto *clientCombo = makeEnvCombo(m_table);
    clientCombo->setCurrentIndex(envToComboIndex(entry.clientEnv));
    m_table->setCellWidget(row, ColClient, clientCombo);

    auto *serverCombo = makeEnvCombo(m_table);
    serverCombo->setCurrentIndex(envToComboIndex(entry.serverEnv));
    m_table->setCellWidget(row, ColServer, serverCombo);

    auto *sha1Item = new QTableWidgetItem(entry.sha1);
    sha1Item->setFlags(sha1Item->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, ColSha1, sha1Item);

    auto *sha512Item = new QTableWidgetItem(entry.sha512);
    sha512Item->setFlags(sha512Item->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, ColSha512, sha512Item);

    auto *sizeItem = new QTableWidgetItem(entry.fileSize > 0 ? QString::number(entry.fileSize) : QString());
    sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, ColSize, sizeItem);

    auto *statusItem = new QTableWidgetItem(entry.statusText);
    statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(row, ColStatus, statusItem);
}

ModFileEntry FilesPage::entryFromRow(int row) const
{
    ModFileEntry e;
    e.path = m_table->item(row, ColPath)->text().trimmed();
    e.localTempPath = m_table->item(row, ColPath)->data(Qt::UserRole).toString();
    e.isLocalImport = m_table->item(row, ColPath)->data(Qt::UserRole + 1).toBool();

    const QString url = m_table->item(row, ColUrl)->text().trimmed();
    e.downloads = url.isEmpty() ? QStringList() : QStringList{url};

    e.clientEnv = comboToEnv(qobject_cast<QComboBox *>(m_table->cellWidget(row, ColClient)));
    e.serverEnv = comboToEnv(qobject_cast<QComboBox *>(m_table->cellWidget(row, ColServer)));

    e.sha1 = m_table->item(row, ColSha1)->text();
    e.sha512 = m_table->item(row, ColSha512)->text();
    e.fileSize = m_table->item(row, ColSize)->text().toLongLong();
    e.hashComputed = !e.sha1.isEmpty() && !e.sha512.isEmpty();
    e.statusText = m_table->item(row, ColStatus)->text();
    return e;
}

void FilesPage::onAddUrls()
{
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, tr("批次新增直連網址"),
        tr("每行貼一個模組直連下載網址："), QString(), &ok);
    if (!ok || text.trimmed().isEmpty())
        return;

    for (const QString &line : text.split('\n', Qt::SkipEmptyParts)) {
        const QString url = line.trimmed();
        if (url.isEmpty())
            continue;
        ModFileEntry e;
        e.downloads = {url};
        const QString fileName = QFileInfo(QUrl(url).path()).fileName();
        e.path = guessPathFromName(fileName.isEmpty() ? tr("未命名檔案.jar") : fileName);
        e.statusText = tr("待處理");
        addRow(e);
    }
    log(tr("已新增網址項目，請按「計算雜湊值與大小」以下載並驗證。"));
}

void FilesPage::onAddLocalFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("選擇模組檔案"), QString(), tr("模組檔案 (*.jar);;所有檔案 (*.*)"));
    for (const QString &path : files) {
        ModFileEntry e;
        e.localTempPath = path;
        e.isLocalImport = true;
        e.path = guessPathFromName(QFileInfo(path).fileName());
        e.statusText = tr("待處理（本機檔案，匯出前請填寫下載網址）");
        addRow(e);
    }
    if (!files.isEmpty())
        log(tr("已新增 %1 個本機檔案，請按「計算雜湊值與大小」；"
               "mrpack 規格要求每個檔案至少一個下載網址，本機檔案請自行於「下載網址」欄補上。").arg(files.size()));
}

void FilesPage::onAddFromSearch()
{
    auto *dialog = new ModSearchDialog(m_data->basic.mcVersion, m_data->basic.loaderType, this);
    connect(dialog, &ModSearchDialog::modsChosen, this, [this](const QVector<ModFileEntry> &entries) {
        for (const auto &e : entries) {
            ModFileEntry entry = e;
            entry.statusText = tr("待處理");
            addRow(entry);
        }
        log(tr("已從模組搜尋加入 %1 個項目，請按「計算雜湊值與大小」以下載並驗證。").arg(entries.size()));
    });
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void FilesPage::onRemoveSelected()
{
    QVector<int> rows;
    for (auto *item : m_table->selectedItems()) {
        if (!rows.contains(item->row()))
            rows.append(item->row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows)
        m_table->removeRow(r);
}

void FilesPage::onComputeAll()
{
    m_queue.clear();
    for (int r = 0; r < m_table->rowCount(); ++r)
        m_queue.append(r);
    if (m_queue.isEmpty()) {
        log(tr("目前沒有任何檔案項目。"));
        return;
    }
    m_queueTotal = m_queue.size();
    m_overallProgress->setRange(0, m_queueTotal);
    m_overallProgress->setValue(0);
    m_stageProgress->setRange(0, 1);
    m_stageProgress->setValue(0);
    m_stageLabel->setText(tr("準備開始..."));
    processNextInQueue();
}

void FilesPage::processNextInQueue()
{
    if (m_queue.isEmpty()) {
        m_overallProgress->setValue(m_queueTotal);
        m_stageProgress->setRange(0, 1);
        m_stageProgress->setValue(1);
        m_stageLabel->setText(tr("全部處理完成"));
        log(tr("全部處理完成。"));
        return;
    }
    const int row = m_queue.takeFirst();
    m_table->setItem(row, ColStatus, new QTableWidgetItem(tr("處理中...")));
    const QString path = m_table->item(row, ColPath)->text();

    const bool isLocal = m_table->item(row, ColPath)->data(Qt::UserRole + 1).toBool();
    const QString localPath = m_table->item(row, ColPath)->data(Qt::UserRole).toString();
    const QString url = m_table->item(row, ColUrl)->text().trimmed();

    if (isLocal) {
        if (localPath.isEmpty()) {
            m_table->item(row, ColStatus)->setText(tr("失敗：找不到本機檔案路徑"));
            processNextInQueue();
            return;
        }
        m_stageLabel->setText(tr("正在計算雜湊值（本機檔案）：%1").arg(path));
        m_stageProgress->setRange(0, 0); // 本機檔案沒有下載階段，雜湊計算沒有逐步進度，用忙碌指示
        auto *watcher = new QFutureWatcher<FileHashResult>(this);
        connect(watcher, &QFutureWatcher<FileHashResult>::finished, this, [this, watcher, row]() {
            applyHashResult(row, watcher->result());
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run([localPath]() { return HashCalculator::calculate(localPath); }));
    } else {
        if (url.isEmpty()) {
            m_table->item(row, ColStatus)->setText(tr("失敗：缺少下載網址"));
            processNextInQueue();
            return;
        }
        const QString fileName = QFileInfo(QUrl(url).path()).fileName();
        m_stageLabel->setText(tr("正在下載：%1").arg(path));
        m_stageProgress->setRange(0, 100);
        m_stageProgress->setValue(0);
        m_pendingRow = row;
        m_downloader->download(url, fileName);
    }
}

void FilesPage::applyHashResult(int row, const FileHashResult &r)
{
    if (r.ok) {
        m_table->item(row, ColSha1)->setText(r.sha1);
        m_table->item(row, ColSha512)->setText(r.sha512);
        m_table->item(row, ColSize)->setText(QString::number(r.fileSize));
        m_table->item(row, ColStatus)->setText(tr("完成"));
        log(tr("列 %1 計算完成（%2 bytes）").arg(row + 1).arg(r.fileSize));
    } else {
        m_table->item(row, ColStatus)->setText(tr("失敗：%1").arg(r.error));
        log(tr("列 %1 失敗：%2").arg(row + 1).arg(r.error));
    }
    m_overallProgress->setValue(m_queueTotal - m_queue.size());
    processNextInQueue();
}

void FilesPage::initializePage()
{
    m_table->setRowCount(0);
    for (const auto &entry : m_data->files)
        addRow(entry);
}

bool FilesPage::validatePage()
{
    m_data->files.clear();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const auto e = entryFromRow(r);
        if (e.path.isEmpty()) {
            QMessageBox::warning(this, tr("欄位不完整"), tr("第 %1 列缺少路徑，請填寫。").arg(r + 1));
            return false;
        }
        m_data->files.append(e);
    }
    return true;
}

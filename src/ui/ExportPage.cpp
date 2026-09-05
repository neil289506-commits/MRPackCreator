#include "ExportPage.h"
#include "../core/MrpackBuilder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

namespace {
struct ExportResult {
    bool ok = false;
    QString error;
    QString outPath;
};
}

ExportPage::ExportPage(ModpackData *data, QWidget *parent)
    : QWizardPage(parent)
    , m_data(data)
{
    setTitle(tr("確認並匯出"));
    setSubTitle(tr("確認以下摘要無誤後，選擇輸出位置並匯出 .mrpack（匯出在背景執行，過程中可以即時看到總進度、階段進度與目前狀態）"));

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName("summaryLabel");
    m_summaryLabel->setWordWrap(true);

    m_outputEdit = new QLineEdit(this);
    auto *browseBtn = new QPushButton(tr("另存為..."), this);
    connect(browseBtn, &QPushButton::clicked, this, &ExportPage::onBrowseOutput);

    auto *outRow = new QHBoxLayout;
    outRow->addWidget(m_outputEdit);
    outRow->addWidget(browseBtn);

    m_exportBtn = new QPushButton(tr("匯出 .mrpack"), this);
    m_exportBtn->setObjectName("primaryButton");
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportPage::onExport);

    m_stageLabel = new QLabel(tr("尚未開始"), this);

    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setRange(0, 1);
    m_overallProgress->setValue(0);
    m_overallProgress->setFormat(tr("總進度：階段 %v / %m"));
    m_overallProgress->setVisible(false);

    m_stageProgress = new QProgressBar(this);
    m_stageProgress->setRange(0, 1);
    m_stageProgress->setValue(0);
    m_stageProgress->setFormat(tr("階段進度：%v / %m"));
    m_stageProgress->setVisible(false);

    m_console = new QPlainTextEdit(this);
    m_console->setObjectName("consoleOutput");
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(2000);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(new QLabel(tr("輸出檔案："), this));
    layout->addLayout(outRow);
    layout->addWidget(m_exportBtn);
    layout->addWidget(m_stageLabel);
    layout->addWidget(m_overallProgress);
    layout->addWidget(m_stageProgress);
    layout->addWidget(new QLabel(tr("匯出 Console："), this));
    layout->addWidget(m_console, 1);
}

void ExportPage::log(const QString &msg)
{
    m_console->appendPlainText(QDateTime::currentDateTime().toString("[hh:mm:ss] ") + msg);
}

void ExportPage::initializePage()
{
    const auto &b = m_data->basic;
    const auto &o = m_data->overrides;

    auto describe = [](const OverrideSelection &s) {
        if (s.instanceDir.isEmpty() || s.selectedPaths.isEmpty())
            return QObject::tr("（無）");
        return QObject::tr("%1 個項目 @ %2").arg(s.selectedPaths.size()).arg(s.instanceDir);
    };

    m_summaryLabel->setText(tr(
        "名稱：%1\n簡介：%2\n版本號：%3\n遊戲版本：%4\n載入器：%5 %6\n檔案數：%7\n"
        "overrides：%8\nclient-overrides：%9\nserver-overrides：%10")
        .arg(b.name, b.summary.isEmpty() ? tr("（無）") : b.summary, b.versionId, b.mcVersion,
             loaderDisplayName(b.loaderType), b.loaderVersion)
        .arg(m_data->files.size())
        .arg(describe(o.overrides), describe(o.clientOverrides), describe(o.serverOverrides)));

    if (m_outputEdit->text().isEmpty()) {
        const QString safeName = b.name.isEmpty() ? "modpack" : b.name;
        m_outputEdit->setText(QDir::homePath() + "/" + safeName + "-" + b.versionId + ".mrpack");
    }
}

void ExportPage::onBrowseOutput()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("匯出 mrpack"), m_outputEdit->text(),
                                                        tr("Modrinth 模組包 (*.mrpack)"));
    if (!path.isEmpty())
        m_outputEdit->setText(path);
}

void ExportPage::setExporting(bool exporting)
{
    m_exporting = exporting;
    m_exportBtn->setEnabled(!exporting);
    m_outputEdit->setEnabled(!exporting);
    m_overallProgress->setVisible(exporting);
    m_stageProgress->setVisible(exporting);
    if (!exporting) {
        m_overallProgress->setRange(0, 1);
        m_overallProgress->setValue(0);
        m_stageProgress->setRange(0, 1);
        m_stageProgress->setValue(0);
    }
}

void ExportPage::onExport()
{
    QString outPath = m_outputEdit->text().trimmed();
    if (outPath.isEmpty()) {
        QMessageBox::warning(this, tr("尚未選擇輸出位置"), tr("請先指定輸出檔案路徑。"));
        return;
    }
    if (!outPath.endsWith(".mrpack", Qt::CaseInsensitive))
        outPath += ".mrpack";

    setExporting(true);
    m_stageLabel->setText(tr("準備開始..."));
    log(tr("開始匯出：%1").arg(outPath));

    // 在背景執行緒跑實際的打包工作，避免大型 overrides 資料夾讓 UI 卡住。
    // MrpackBuilder 在此背景執行緒中建立（不設 parent），三個訊號都透過跨執行緒連線
    // （sender 在背景執行緒、receiver 用 qApp 代表 GUI 執行緒，Qt 自動判定為 QueuedConnection）
    // 安全地送回 GUI 執行緒即時更新總進度／階段進度／目前狀態文字。
    const ModpackData dataCopy = *m_data; // 複製一份給背景執行緒使用，避免跨執行緒共用同一物件
    QPointer<ExportPage> safeThis(this);

    auto future = QtConcurrent::run([dataCopy, outPath, safeThis]() -> ExportResult {
        ExportResult res;
        res.outPath = outPath;

        MrpackBuilder builder; // 建立在背景執行緒，不掛 parent
        QObject::connect(&builder, &MrpackBuilder::progress, qApp, [safeThis](const QString &msg) {
            if (safeThis)
                safeThis->log(msg);
        });
        QObject::connect(&builder, &MrpackBuilder::stageChanged, qApp,
                          [safeThis](int cur, int total, const QString &name) {
            if (!safeThis)
                return;
            safeThis->m_overallProgress->setRange(0, total);
            safeThis->m_overallProgress->setValue(cur);
            safeThis->m_stageLabel->setText(QObject::tr("階段 %1 / %2：%3").arg(cur).arg(total).arg(name));
        });
        QObject::connect(&builder, &MrpackBuilder::stageProgress, qApp, [safeThis](int cur, int total) {
            if (!safeThis)
                return;
            safeThis->m_stageProgress->setRange(0, total > 0 ? total : 1);
            safeThis->m_stageProgress->setValue(cur);
        });

        res.ok = builder.exportMrpack(dataCopy, outPath, res.error);
        return res;
    });

    auto *watcher = new QFutureWatcher<ExportResult>(this);
    connect(watcher, &QFutureWatcher<ExportResult>::finished, this, [this, watcher, safeThis]() {
        const auto res = watcher->result();
        watcher->deleteLater();
        if (!safeThis)
            return;

        setExporting(false);
        if (res.ok) {
            m_exportSucceeded = true;
            m_stageLabel->setText(tr("匯出成功"));
            log(tr("成功匯出：%1").arg(res.outPath));
            QMessageBox::information(this, tr("匯出成功"), tr("已成功匯出至：\n%1").arg(res.outPath));
        } else {
            m_stageLabel->setText(tr("匯出失敗"));
            log(tr("匯出失敗：%1").arg(res.error));
            QMessageBox::critical(this, tr("匯出失敗"), res.error);
        }
    });
    watcher->setFuture(future);
}

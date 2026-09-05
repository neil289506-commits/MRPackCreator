#include "BasicInfoPage.h"
#include "../network/VersionFetcher.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QMessageBox>
#include <QDateTime>

BasicInfoPage::BasicInfoPage(ModpackData *data, QWidget *parent)
    : QWizardPage(parent)
    , m_data(data)
    , m_fetcher(new VersionFetcher(this))
{
    setTitle(tr("基本資訊"));
    setSubTitle(tr("填寫模組包名稱、簡介、遊戲版本與載入器版本"));

    m_nameEdit = new QLineEdit(this);
    m_summaryEdit = new QLineEdit(this);
    m_versionIdEdit = new QLineEdit(this);
    m_versionIdEdit->setPlaceholderText(tr("例如 1.0.0"));

    m_mcVersionCombo = new QComboBox(this);
    m_mcVersionCombo->setEditable(true);
    m_mcVersionCombo->addItem(tr("（載入中...）"));

    m_loaderTypeCombo = new QComboBox(this);
    m_loaderTypeCombo->addItem("Fabric", static_cast<int>(LoaderType::Fabric));
    m_loaderTypeCombo->addItem("Forge", static_cast<int>(LoaderType::Forge));
    m_loaderTypeCombo->addItem("NeoForge", static_cast<int>(LoaderType::NeoForge));
    m_loaderTypeCombo->addItem("Quilt", static_cast<int>(LoaderType::Quilt));

    m_loaderVersionCombo = new QComboBox(this);
    m_loaderVersionCombo->setEditable(true);

    auto *form = new QFormLayout;
    form->addRow(tr("模組包名稱*："), m_nameEdit);
    form->addRow(tr("簡介："), m_summaryEdit);
    form->addRow(tr("模組包版本號*："), m_versionIdEdit);
    form->addRow(tr("遊戲版本*："), m_mcVersionCombo);
    form->addRow(tr("載入器類型*："), m_loaderTypeCombo);
    form->addRow(tr("載入器版本*："), m_loaderVersionCombo);

    m_console = new QPlainTextEdit(this);
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(500);
    m_console->setFixedHeight(120);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("版本抓取紀錄："), this));
    layout->addWidget(m_console);

    registerField("modpack.name*", m_nameEdit);
    registerField("modpack.versionId*", m_versionIdEdit);

    connect(m_fetcher, &VersionFetcher::progress, this, &BasicInfoPage::log);
    connect(m_fetcher, &VersionFetcher::fetchError, this, &BasicInfoPage::log);

    connect(m_fetcher, &VersionFetcher::gameVersionsReady, this, [this](const QStringList &versions) {
        m_mcVersionCombo->clear();
        m_mcVersionCombo->addItems(versions);
        log(tr("已取得 %1 個遊戲版本").arg(versions.size()));
        refreshLoaderVersions();
    });

    connect(m_fetcher, &VersionFetcher::loaderVersionsReady, this, [this](LoaderType type, const QStringList &versions) {
        // 若載入器類型下拉選單在抓取期間被使用者切換，忽略過期的回應
        if (type != static_cast<LoaderType>(m_loaderTypeCombo->currentData().toInt()))
            return;
        m_loaderVersionCombo->clear();
        m_loaderVersionCombo->addItems(versions);
        log(tr("已取得 %1 個 %2 載入器版本").arg(versions.size()).arg(loaderDisplayName(type)));
    });

    connect(m_mcVersionCombo, &QComboBox::currentTextChanged, this, [this](const QString &) {
        refreshLoaderVersions();
        emit completeChanged();
    });
    connect(m_loaderVersionCombo, &QComboBox::currentTextChanged, this, [this](const QString &) {
        emit completeChanged();
    });
    connect(m_loaderTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicInfoPage::onLoaderTypeChanged);

    m_fetcher->fetchGameVersions(false);
}

void BasicInfoPage::log(const QString &msg)
{
    m_console->appendPlainText(QDateTime::currentDateTime().toString("[hh:mm:ss] ") + msg);
}

void BasicInfoPage::onLoaderTypeChanged()
{
    m_loaderVersionCombo->clear();
    refreshLoaderVersions();
}

void BasicInfoPage::refreshLoaderVersions()
{
    const auto type = static_cast<LoaderType>(m_loaderTypeCombo->currentData().toInt());
    const QString mc = m_mcVersionCombo->currentText().trimmed();
    if (mc.isEmpty() || mc.startsWith(tr("（載入中")))
        return;
    m_fetcher->fetchLoaderVersions(type, mc);
}

bool BasicInfoPage::isComplete() const
{
    return !m_nameEdit->text().trimmed().isEmpty()
        && !m_versionIdEdit->text().trimmed().isEmpty()
        && !m_mcVersionCombo->currentText().trimmed().isEmpty()
        && !m_loaderVersionCombo->currentText().trimmed().isEmpty();
}

bool BasicInfoPage::validatePage()
{
    if (!isComplete()) {
        QMessageBox::warning(this, tr("欄位未完成"), tr("請完整填寫名稱、版本號、遊戲版本與載入器版本。"));
        return false;
    }
    m_data->basic.name = m_nameEdit->text().trimmed();
    m_data->basic.summary = m_summaryEdit->text().trimmed();
    m_data->basic.versionId = m_versionIdEdit->text().trimmed();
    m_data->basic.mcVersion = m_mcVersionCombo->currentText().trimmed();
    m_data->basic.loaderType = static_cast<LoaderType>(m_loaderTypeCombo->currentData().toInt());
    m_data->basic.loaderVersion = m_loaderVersionCombo->currentText().trimmed();
    return true;
}

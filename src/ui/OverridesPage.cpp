#include "OverridesPage.h"
#include "OverrideTreeSelector.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QTabWidget>

OverridesPage::OverridesPage(ModpackData *data, QWidget *parent)
    : QWizardPage(parent)
    , m_data(data)
{
    setTitle(tr("覆蓋資料夾（Overrides）"));
    setSubTitle(tr("每個分頁都先選擇一個實例資料夾（可以是任何模組包工作目錄，不限定 .minecraft），"
                    "再勾選要包含哪些子項目；勾選整個資料夾就會連同底下所有內容一起包含。"));

    auto *tabs = new QTabWidget(this);

    m_overridesSel = new OverrideTreeSelector(this);
    m_clientSel = new OverrideTreeSelector(this);
    m_serverSel = new OverrideTreeSelector(this);

    auto wrapWithNote = [this](OverrideTreeSelector *sel, const QString &note) {
        auto *w = new QWidget(this);
        auto *l = new QVBoxLayout(w);
        auto *label = new QLabel(note, w);
        label->setWordWrap(true);
        l->addWidget(label);
        l->addWidget(sel, 1);
        return w;
    };

    tabs->addTab(wrapWithNote(m_overridesSel, tr("全局 overrides：客戶端與伺服器端都會套用，例如共用的 config/、resourcepacks/")),
                 tr("overrides"));
    tabs->addTab(wrapWithNote(m_clientSel, tr("client-overrides：只有玩家端安裝時會套用，例如 options.txt、shaderpacks/")),
                 tr("client-overrides"));
    tabs->addTab(wrapWithNote(m_serverSel, tr("server-overrides：只有伺服器端安裝時會套用，例如 server.properties、datapacks/")),
                 tr("server-overrides"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
}

void OverridesPage::initializePage()
{
    if (!m_data->overrides.overrides.instanceDir.isEmpty()) {
        m_overridesSel->setInstanceDir(m_data->overrides.overrides.instanceDir);
        m_overridesSel->setCheckedPaths(m_data->overrides.overrides.selectedPaths);
    }
    if (!m_data->overrides.clientOverrides.instanceDir.isEmpty()) {
        m_clientSel->setInstanceDir(m_data->overrides.clientOverrides.instanceDir);
        m_clientSel->setCheckedPaths(m_data->overrides.clientOverrides.selectedPaths);
    }
    if (!m_data->overrides.serverOverrides.instanceDir.isEmpty()) {
        m_serverSel->setInstanceDir(m_data->overrides.serverOverrides.instanceDir);
        m_serverSel->setCheckedPaths(m_data->overrides.serverOverrides.selectedPaths);
    }
}

bool OverridesPage::validatePage()
{
    m_data->overrides.overrides.instanceDir = m_overridesSel->instanceDir();
    m_data->overrides.overrides.selectedPaths = m_overridesSel->checkedPaths();

    m_data->overrides.clientOverrides.instanceDir = m_clientSel->instanceDir();
    m_data->overrides.clientOverrides.selectedPaths = m_clientSel->checkedPaths();

    m_data->overrides.serverOverrides.instanceDir = m_serverSel->instanceDir();
    m_data->overrides.serverOverrides.selectedPaths = m_serverSel->checkedPaths();
    return true;
}

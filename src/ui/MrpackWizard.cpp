#include "MrpackWizard.h"
#include "BasicInfoPage.h"
#include "FilesPage.h"
#include "OverridesPage.h"
#include "ExportPage.h"

MrpackWizard::MrpackWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("Mrpack 製作工具"));
    setWizardStyle(QWizard::ModernStyle); // 避免 Windows AeroStyle 在部分環境下 DWM 合成失敗導致畫面全黑
    setMinimumSize(820, 640);
    setOption(QWizard::NoBackButtonOnStartPage, true);

    setPage(PageBasicInfo, new BasicInfoPage(&m_data, this));
    setPage(PageFiles, new FilesPage(&m_data, this));
    setPage(PageOverrides, new OverridesPage(&m_data, this));
    setPage(PageExport, new ExportPage(&m_data, this));
}

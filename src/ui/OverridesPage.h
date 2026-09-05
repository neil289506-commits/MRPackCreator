#pragma once
#include <QWizardPage>
#include "../core/ModpackData.h"

class OverrideTreeSelector;

// 第三頁：全局 overrides、client-overrides、server-overrides
// 每一類都是「選擇實例資料夾 + 勾選要包含的子項目」，而不是直接整包一個資料夾。
class OverridesPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit OverridesPage(ModpackData *data, QWidget *parent = nullptr);

    void initializePage() override;
    bool validatePage() override;

private:
    ModpackData *m_data;
    OverrideTreeSelector *m_overridesSel;
    OverrideTreeSelector *m_clientSel;
    OverrideTreeSelector *m_serverSel;
};

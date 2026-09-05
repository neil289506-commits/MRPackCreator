#pragma once
#include <QWizardPage>
#include "../core/ModpackData.h"

class QLineEdit;
class QPlainTextEdit;
class QComboBox;
class VersionFetcher;

// 第一頁：名稱、簡介、模組包版本號、遊戲版本、載入器類型、載入器版本
class BasicInfoPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit BasicInfoPage(ModpackData *data, QWidget *parent = nullptr);

    bool isComplete() const override;
    bool validatePage() override;

private slots:
    void onLoaderTypeChanged();
    void refreshLoaderVersions();

private:
    void log(const QString &msg);

    ModpackData *m_data;
    VersionFetcher *m_fetcher;

    QLineEdit *m_nameEdit;
    QLineEdit *m_summaryEdit;
    QLineEdit *m_versionIdEdit;
    QComboBox *m_mcVersionCombo;
    QComboBox *m_loaderTypeCombo;
    QComboBox *m_loaderVersionCombo;
    QPlainTextEdit *m_console;
};

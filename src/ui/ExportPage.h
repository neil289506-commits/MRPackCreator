#pragma once
#include <QWizardPage>
#include <QPointer>
#include "../core/ModpackData.h"

class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QLabel;
class QPushButton;

// 第四頁：確認摘要、選擇輸出路徑並在背景執行緒執行匯出（避免 UI 卡住）。
// 進度顯示採三段式：
//   - m_overallProgress：總進度（目前是第幾個匯出階段，例如 2/4）
//   - m_stageProgress：階段進度（目前這個階段內已處理幾個檔案）
//   - m_stageLabel：目前階段文字說明
// 同時也把每一步驟即時輸出到下方 Console。
class ExportPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit ExportPage(ModpackData *data, QWidget *parent = nullptr);

    void initializePage() override;

private slots:
    void onBrowseOutput();
    void onExport();

private:
    void log(const QString &msg);
    void setExporting(bool exporting);

    ModpackData *m_data;
    QLabel *m_summaryLabel;
    QLineEdit *m_outputEdit;
    QPushButton *m_exportBtn;

    QLabel *m_stageLabel;
    QProgressBar *m_overallProgress;
    QProgressBar *m_stageProgress;

    QPlainTextEdit *m_console;
    bool m_exporting = false;
    bool m_exportSucceeded = false;
};

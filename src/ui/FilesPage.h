#pragma once
#include <QWizardPage>
#include <QVector>
#include "../core/ModpackData.h"

class QTableWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QLabel;
class FileDownloader;
struct FileHashResult;

// 第二頁：批次匯入模組檔案（直連網址或本機檔案），並計算 sha1/sha512/檔案大小。
class FilesPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit FilesPage(ModpackData *data, QWidget *parent = nullptr);

    void initializePage() override;
    bool validatePage() override;

private slots:
    void onAddUrls();
    void onAddLocalFiles();
    void onAddFromSearch();
    void onRemoveSelected();
    void onComputeAll();

private:
    int addRow(const ModFileEntry &entry);
    void setRowFromEntry(int row, const ModFileEntry &entry);
    ModFileEntry entryFromRow(int row) const;
    void log(const QString &msg);
    void processNextInQueue();
    void applyHashResult(int row, const FileHashResult &r);
    QString guessPathFromName(const QString &fileName) const;

    ModpackData *m_data;
    FileDownloader *m_downloader;

    QTableWidget *m_table;
    QPlainTextEdit *m_console;

    // 三段式進度顯示：總進度（佇列中已完成幾筆）、階段進度（目前這筆下載的位元組進度）、
    // 目前階段文字說明（正在下載/正在計算雜湊的是哪個檔案）
    QProgressBar *m_overallProgress;
    QProgressBar *m_stageProgress;
    QLabel *m_stageLabel;

    QVector<int> m_queue;
    int m_queueTotal = 0;
    int m_pendingRow = -1; // 目前正在下載中的列（佇列為循序處理，同一時間只會有一筆）
};

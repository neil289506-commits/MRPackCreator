#pragma once
#include <QDialog>
#include <QVector>
#include "../core/ModpackData.h"
#include "../network/ModrinthApi.h"
#include "../network/CurseForgeApi.h"

class QLineEdit;
class QComboBox;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;

// 讓使用者搜尋 Modrinth 或 CurseForge 上的模組，選好對應遊戲版本/載入器的檔案後，
// 直接加入到 FilesPage 的檔案清單（跟直連網址項目走同一套下載＋雜湊計算流程，
// 只是網址來源改成從 API 查詢取得，不需要使用者自己去找連結）。
class ModSearchDialog : public QDialog
{
    Q_OBJECT
public:
    // mcVersion / loaderType / loaderVersion 取自精靈第一頁已填好的基本資訊，
    // 用來過濾出跟目前模組包相容的檔案。
    explicit ModSearchDialog(const QString &mcVersion, LoaderType loaderType, QWidget *parent = nullptr);

signals:
    // 使用者按下「加入清單」後，回傳要加入 FilesPage 的項目（皆為直連網址型態）
    void modsChosen(const QVector<ModFileEntry> &entries);

private slots:
    void onSearch();
    void onResultSelected(QListWidgetItem *item);
    void onFileSelected(QListWidgetItem *item);
    void onAddChosenFile();
    void onSetCurseForgeApiKey();

private:
    void log(const QString &msg);
    int curseForgeLoaderTypeId() const; // CurseForge modLoaderType 數值對照

    QString m_mcVersion;
    LoaderType m_loaderType;

    QComboBox *m_providerCombo;
    QLineEdit *m_queryEdit;
    QPushButton *m_searchBtn;
    QListWidget *m_resultList;
    QListWidget *m_fileList;
    QPushButton *m_addBtn;
    QPushButton *m_apiKeyBtn;
    QLabel *m_statusLabel;

    ModrinthApi *m_modrinth;
    CurseForgeApi *m_curseforge;

    // 暫存目前選到、準備加入清單的檔案資訊
    QString m_pendingPath;
    QString m_pendingUrl;
    qint64 m_pendingCfModId = 0;
    qint64 m_pendingCfFileId = 0;
    bool m_pendingIsCurseForge = false;
};

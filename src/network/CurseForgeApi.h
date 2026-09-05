#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QVector>

struct CurseForgeModSummary {
    qint64  modId = 0;
    QString slug;
    QString name;
    QString summary;
    QString iconUrl;
    int     downloadCount = 0;
};

// 對應 /v1/mods/{modId}/files 回傳的單一檔案項目。fileDate 刻意留成
// QString 而不是 QDateTime——CurseForge 回傳的是 ISO 8601
// （例如 "2024-05-01T12:34:56.000Z"），UI 端目前只需要直接顯示文字，
// 需要排序/計算時再由呼叫端自行用 QDateTime::fromString 轉換，
// 避免這裡的解析失敗（時區格式異常等）連帶讓整包檔案清單解析中斷。
struct CurseForgeFileInfo {
    qint64  fileId = 0;
    QString displayName;
    QString fileDate;
    qint64  fileLength = 0;
};

class CurseForgeApi : public QObject
{
    Q_OBJECT
public:
    explicit CurseForgeApi(QObject *parent = nullptr);

    void setApiKey(const QString &apiKey) { m_apiKey = apiKey; }
    bool hasApiKey() const { return !m_apiKey.isEmpty(); }

    // classId: 6=Mod, 12=ResourcePack, 6945=DataPack
    void searchMods(const QString &query,
                    const QString &gameVersion,
                    int modLoaderType,
                    int classId = 6);

    // 取得指定模組在特定遊戲版本／載入器下的可用檔案清單。
    // UI 選好模組（拿到 modId）之後，要先呼叫這個方法列出檔案讓使用者
    // 選一個，才會有合法的 fileId 可以傳給 fetchDownloadUrl——modId
    // 本身不能拿來下載，CurseForge 的下載連結一定要 modId+fileId 兩個
    // 一起才查得到，這就是原本「無效的 modId 或 fileId」錯誤的成因。
    void fetchModFiles(qint64 modId, const QString &gameVersion, int modLoaderType = 0);

    void fetchDownloadUrl(qint64 modId, qint64 fileId);

signals:
    void searchFinished(const QVector<CurseForgeModSummary> &results);
    void modFilesFinished(qint64 modId, const QVector<CurseForgeFileInfo> &files);
    void downloadUrlFinished(const QString &url);
    void errorOccurred(const QString &message);

private:
    QNetworkAccessManager *m_nam;
    QString m_apiKey;
    static constexpr const char *kBaseUrl = "https://api.curseforge.com/v1";
};

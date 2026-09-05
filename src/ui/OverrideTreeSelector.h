#pragma once
#include <QWidget>
#include <QStringList>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

// 讓使用者先選擇一個「實例資料夾」（不強制假設是 .minecraft，可以是任何
// 模組包工作目錄），再用勾選樹狀清單挑選要包含哪些子項目，
// 而不是把整個資料夾整包塞進 overrides。
class OverrideTreeSelector : public QWidget
{
    Q_OBJECT
public:
    explicit OverrideTreeSelector(QWidget *parent = nullptr);

    void setInstanceDir(const QString &dir);
    QString instanceDir() const { return m_instanceDir; }

    // 取得目前勾選的相對路徑清單（已收斂：完整勾選的資料夾只會回傳資料夾本身一筆）
    QStringList checkedPaths() const;
    // 還原先前的勾選狀態（會依需要展開/建立節點）
    void setCheckedPaths(const QStringList &paths);

private slots:
    void onBrowse();
    void onRefresh();
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemChanged(QTreeWidgetItem *item, int column);

private:
    void populateRoot();
    void populateChildren(QTreeWidgetItem *parentItem, const QString &absPath, Qt::CheckState initialState);
    void setDescendantsCheckState(QTreeWidgetItem *item, Qt::CheckState state);
    void updateAncestorsCheckState(QTreeWidgetItem *item);
    void collectChecked(QTreeWidgetItem *item, const QString &relPrefix, QStringList &out) const;
    QTreeWidgetItem *ensureChildPopulated(QTreeWidgetItem *parent, const QString &name, const QString &absPath, bool isDir);

    QLineEdit *m_dirEdit;
    QTreeWidget *m_tree;
    QString m_instanceDir;
    bool m_updating = false;
};

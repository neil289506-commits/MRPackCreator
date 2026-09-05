#include "OverrideTreeSelector.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QStyle>

namespace {
constexpr int RoleAbsPath = Qt::UserRole;
constexpr int RoleIsDir = Qt::UserRole + 1;
constexpr int RolePopulated = Qt::UserRole + 2;
}

OverrideTreeSelector::OverrideTreeSelector(QWidget *parent)
    : QWidget(parent)
{
    m_dirEdit = new QLineEdit(this);
    m_dirEdit->setReadOnly(true);
    m_dirEdit->setPlaceholderText(tr("尚未選擇實例資料夾"));

    auto *browseBtn = new QPushButton(tr("選擇實例資料夾..."), this);
    auto *refreshBtn = new QPushButton(tr("重新整理"), this);
    connect(browseBtn, &QPushButton::clicked, this, &OverrideTreeSelector::onBrowse);
    connect(refreshBtn, &QPushButton::clicked, this, &OverrideTreeSelector::onRefresh);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(m_dirEdit, 1);
    topRow->addWidget(browseBtn);
    topRow->addWidget(refreshBtn);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("項目")});
    m_tree->setColumnCount(1);
    m_tree->header()->setSectionResizeMode(QHeaderView::Stretch);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(topRow);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemExpanded, this, &OverrideTreeSelector::onItemExpanded);
    connect(m_tree, &QTreeWidget::itemChanged, this, &OverrideTreeSelector::onItemChanged);
}

void OverrideTreeSelector::onBrowse()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("選擇實例資料夾"));
    if (!dir.isEmpty())
        setInstanceDir(dir);
}

void OverrideTreeSelector::onRefresh()
{
    populateRoot();
}

void OverrideTreeSelector::setInstanceDir(const QString &dir)
{
    m_instanceDir = dir;
    m_dirEdit->setText(dir);
    populateRoot();
}

void OverrideTreeSelector::populateRoot()
{
    m_updating = true;
    m_tree->clear();
    m_updating = false;

    if (m_instanceDir.isEmpty() || !QDir(m_instanceDir).exists())
        return;

    QDir dir(m_instanceDir);
    dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    dir.setSorting(QDir::DirsFirst | QDir::Name);

    m_updating = true;
    for (const QFileInfo &fi : dir.entryInfoList()) {
        ensureChildPopulated(nullptr, fi.fileName(), fi.absoluteFilePath(), fi.isDir());
    }
    m_updating = false;
}

QTreeWidgetItem *OverrideTreeSelector::ensureChildPopulated(QTreeWidgetItem *parent, const QString &name,
                                                             const QString &absPath, bool isDir)
{
    auto *item = new QTreeWidgetItem();
    item->setText(0, name + (isDir ? "/" : ""));
    item->setIcon(0, style()->standardIcon(isDir ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
    item->setData(0, RoleAbsPath, absPath);
    item->setData(0, RoleIsDir, isDir);
    item->setData(0, RolePopulated, false);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, Qt::Unchecked);
    if (isDir)
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

    if (parent)
        parent->addChild(item);
    else
        m_tree->addTopLevelItem(item);
    return item;
}

void OverrideTreeSelector::populateChildren(QTreeWidgetItem *parentItem, const QString &absPath, Qt::CheckState initialState)
{
    if (parentItem->data(0, RolePopulated).toBool())
        return;

    QDir dir(absPath);
    dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    dir.setSorting(QDir::DirsFirst | QDir::Name);

    m_updating = true;
    for (const QFileInfo &fi : dir.entryInfoList()) {
        auto *child = ensureChildPopulated(parentItem, fi.fileName(), fi.absoluteFilePath(), fi.isDir());
        // 展開之前若父項目已經是「整個勾選」或「整個未勾選」，子項目沿用同樣狀態，
        // 這樣使用者先勾選資料夾、之後才展開看細節時，畫面顯示才會一致。
        if (initialState != Qt::PartiallyChecked)
            child->setCheckState(0, initialState);
    }
    parentItem->setData(0, RolePopulated, true);
    m_updating = false;
}

void OverrideTreeSelector::onItemExpanded(QTreeWidgetItem *item)
{
    if (!item->data(0, RoleIsDir).toBool())
        return;
    populateChildren(item, item->data(0, RoleAbsPath).toString(), item->checkState(0));
}

void OverrideTreeSelector::setDescendantsCheckState(QTreeWidgetItem *item, Qt::CheckState state)
{
    for (int i = 0; i < item->childCount(); ++i) {
        auto *child = item->child(i);
        child->setCheckState(0, state);
        if (child->data(0, RoleIsDir).toBool() && child->data(0, RolePopulated).toBool())
            setDescendantsCheckState(child, state);
    }
}

void OverrideTreeSelector::updateAncestorsCheckState(QTreeWidgetItem *item)
{
    while (item) {
        int checkedCount = 0, uncheckedCount = 0;
        for (int i = 0; i < item->childCount(); ++i) {
            switch (item->child(i)->checkState(0)) {
            case Qt::Checked:   ++checkedCount; break;
            case Qt::Unchecked: ++uncheckedCount; break;
            default: break; // Partially checked child -> parent 一定也是 Partially checked
            }
        }
        Qt::CheckState newState;
        if (item->childCount() == 0) {
            newState = item->checkState(0); // 沒有已載入的子項目時，保留自身現有狀態
        } else if (checkedCount == item->childCount()) {
            newState = Qt::Checked;
        } else if (uncheckedCount == item->childCount()) {
            newState = Qt::Unchecked;
        } else {
            newState = Qt::PartiallyChecked;
        }
        item->setCheckState(0, newState);
        item = item->parent();
    }
}

void OverrideTreeSelector::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0 || m_updating)
        return;

    m_updating = true;
    const Qt::CheckState state = item->checkState(0);
    if (item->data(0, RoleIsDir).toBool() && item->data(0, RolePopulated).toBool())
        setDescendantsCheckState(item, state);
    m_updating = false;

    updateAncestorsCheckState(item->parent());
}

void OverrideTreeSelector::collectChecked(QTreeWidgetItem *item, const QString &relPrefix, QStringList &out) const
{
    const Qt::CheckState state = item->checkState(0);
    if (state == Qt::Checked) {
        out.append(relPrefix);
    } else if (state == Qt::PartiallyChecked) {
        for (int i = 0; i < item->childCount(); ++i) {
            auto *child = item->child(i);
            const QFileInfo fi(child->data(0, RoleAbsPath).toString());
            collectChecked(child, relPrefix + "/" + fi.fileName(), out);
        }
    }
    // Unchecked -> 不收集
}

QStringList OverrideTreeSelector::checkedPaths() const
{
    QStringList out;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *item = m_tree->topLevelItem(i);
        const QFileInfo fi(item->data(0, RoleAbsPath).toString());
        collectChecked(item, fi.fileName(), out);
    }
    return out;
}

void OverrideTreeSelector::setCheckedPaths(const QStringList &paths)
{
    if (paths.isEmpty() || m_instanceDir.isEmpty())
        return;

    for (const QString &relPath : paths) {
        const QStringList parts = relPath.split('/', Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        QTreeWidgetItem *current = nullptr;
        QString accumulatedAbs = m_instanceDir;
        for (int depth = 0; depth < parts.size(); ++depth) {
            const QString &name = parts[depth];
            accumulatedAbs = QDir(accumulatedAbs).filePath(name);

            QTreeWidgetItem *found = nullptr;
            const int count = current ? current->childCount() : m_tree->topLevelItemCount();
            for (int i = 0; i < count; ++i) {
                QTreeWidgetItem *candidate = current ? current->child(i) : m_tree->topLevelItem(i);
                if (QFileInfo(candidate->data(0, RoleAbsPath).toString()).fileName() == name) {
                    found = candidate;
                    break;
                }
            }
            if (!found)
                break; // 找不到對應節點（可能實例內容已變動），放棄這一筆
            current = found;

            if (depth < parts.size() - 1 && current->data(0, RoleIsDir).toBool())
                populateChildren(current, current->data(0, RoleAbsPath).toString(), Qt::Unchecked);
        }
        if (current) {
            m_updating = true;
            current->setCheckState(0, Qt::Checked);
            if (current->data(0, RoleIsDir).toBool() && current->data(0, RolePopulated).toBool())
                setDescendantsCheckState(current, Qt::Checked);
            m_updating = false;
            updateAncestorsCheckState(current->parent());
        }
    }
}

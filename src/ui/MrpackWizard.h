#pragma once
#include <QWizard>
#include "../core/ModpackData.h"

// 整個工具的主流程：QWizard 依序引導使用者完成
// 基本資訊 -> 匯入檔案 -> 覆蓋資料夾 -> 確認並匯出
class MrpackWizard : public QWizard
{
    Q_OBJECT
public:
    enum PageId { PageBasicInfo, PageFiles, PageOverrides, PageExport };

    explicit MrpackWizard(QWidget *parent = nullptr);

private:
    ModpackData m_data;
};

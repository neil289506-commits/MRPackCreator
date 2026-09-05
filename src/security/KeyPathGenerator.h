#pragma once
#include <QString>

// 產生金鑰檔（.pem 內容）要存放的隨機路徑，多層隨機資料夾疊加，
// 混在系統合法資料夾裡不顯眼。
class KeyPathGenerator
{
public:
    static void generatePaths(QString &outPrivateKeyPath, QString &outPublicKeyPath);

private:
    static QString pickRandomBaseDir();
    static QString randomSegment(int length);
};

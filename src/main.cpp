#include <QApplication>
#include <QFile>
#include <QIcon>
#include "ui/MrpackWizard.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("MrpackMaker");
    QApplication::setOrganizationName("MrpackMaker");

    // 套用深色現代風格（QSS，內容在 resources/style.qss，透過 Qt 資源系統打包進執行檔）
    QFile styleFile(":/style.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    QApplication::setWindowIcon(QIcon(":/app.ico"));

    MrpackWizard wizard;
    wizard.show();

    return app.exec();
}

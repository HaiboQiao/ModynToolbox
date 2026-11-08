#include "ui/workbench.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>
#include <QLockFile>
#include <QDebug>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("ModynToolbox");
    app.setApplicationVersion("0.2.0");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"data-root", "Workspace data directory", "path"});
    parser.addOption({"packages-root", "Installed tool packages", "path"});
    parser.process(app);
    const auto dataRoot = parser.value("data-root");
    if (dataRoot.isEmpty() || !QDir().mkpath(dataRoot + "/host")) {
        QMessageBox::critical(nullptr, QStringLiteral("数据目录"), QStringLiteral("请通过 --data-root 指定可写的数据目录。"));
        return 2;
    }
    QLockFile lock(QDir(dataRoot).absoluteFilePath("host/instance.lock"));
    if (!lock.tryLock(0)) {
        QMessageBox::information(nullptr, QStringLiteral("工作区已占用"), QStringLiteral("该数据目录已有运行实例，或锁文件不可写。"));
        return 3;
    }
    const auto catalog = readCatalog(parser.value("packages-root"));
    for (const auto &error : catalog.errors) qWarning().noquote() << error;
    Workbench window(catalog.tools);
    window.show();
    return app.exec();
}

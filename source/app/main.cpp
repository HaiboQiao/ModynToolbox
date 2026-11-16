#include "platform/windows/single_instance.h"
#include "platform/windows/startup_registration.h"
#include <QPointer>
#include "ui/workbench.h"
#include "catalog/package_manager.h"
#include "ui/components/icons.h"
#include "automation.h"
#include "core/files.h"
#include "storage/data_transfer.h"
#include <QApplication>
#include <QFontDatabase>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>
#include <QLockFile>
#include <QDebug>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("ModynToolbox");
    app.setWindowIcon(brandIcon());
    app.setApplicationVersion(QStringLiteral(MODYN_VERSION));
    if (QGuiApplication::platformName() == "offscreen") {
        const auto fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
        QFontDatabase::addApplicationFont(fonts + "segoeui.ttf");
        QFontDatabase::addApplicationFont(fonts + "msyh.ttc");
    }
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"data-root", "Workspace data directory", "path"});
    parser.addOption({"packages-root", "Installed tool packages", "path"});
    parser.addOption({"tool-package", "Run one complete tool package with the real host services", "path"});
    parser.addOption({"autostart", "Started after Windows logon"});
    parser.addOption({"automation", "Offscreen developer acceptance script", "path"});
    parser.process(app);
    const auto base = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
    auto dataRoot =
        QDir(parser.isSet("data-root") ? parser.value("data-root") : base + "/data").absolutePath();
    auto packagesRoot =
        QDir(parser.isSet("packages-root") ? parser.value("packages-root") : base + "/tools")
            .absolutePath();
    auto fail = [&](const QString &message, int code) {
        if (parser.isSet("automation") || QGuiApplication::platformName() == "offscreen")
            qCritical().noquote() << message;
        else
            QMessageBox::critical(nullptr, QStringLiteral("ModynToolbox"), message);
        return code;
    };
    const bool isolated=parser.isSet("automation")||parser.isSet("tool-package")||QGuiApplication::platformName()=="offscreen";
    SingleInstance instance;
    if(!isolated){
        const auto result=instance.acquire(!parser.isSet("autostart"));
        if(result==SingleInstance::Result::Secondary)return 0;
        if(result!=SingleInstance::Result::Primary)return fail(instance.errorString(),7);
    }
    auto writable = [](const QString &root) {
        if (!QDir().mkpath(root + "/host"))
            return false;
        QTemporaryFile probe(root + "/host/.write-check-XXXXXX");
        if (!probe.open())
            return false;
        if (QFileInfo::exists(root + "/host/state.sqlite")) {
            QFile file(root + "/host/state.sqlite");
            if (!file.open(QIODevice::ReadWrite))
                return false;
        }
        return true;
    };
    while (!writable(dataRoot)) {
        if (QGuiApplication::platformName() == "offscreen")
            return fail(QStringLiteral("数据目录不可写"), 2);
        QMessageBox::information(
            nullptr, QStringLiteral("选择数据位置"),
            QStringLiteral("当前数据目录不可写。将在所选位置建立 ModynToolbox-data 文件夹。"));
        const auto parent =
            QFileDialog::getExistingDirectory(nullptr, QStringLiteral("选择可写的数据位置"));
        if (parent.isEmpty())
            return 2;
        dataRoot = QDir(parent).absoluteFilePath("ModynToolbox-data");
    }
    QLockFile lock(dataRoot + "/host/instance.lock");
    if (!lock.tryLock(0))
        return fail(QStringLiteral("该数据目录已被占用，或锁文件不可写。"), 3);
    const auto recovery = DataTransfer::recover(dataRoot);
    if (!recovery.isEmpty())
        return fail(recovery, 4);
    auto catalog = readCatalog(packagesRoot);
    if(parser.isSet("tool-package")){
        const auto path=QDir(parser.value("tool-package")).absolutePath();QString error;
        auto tool=definitionFromManifest(path,Files::readObject(path+"/manifest.json",65536,&error),&error);
        if(error.isEmpty())error=PackageManager(packagesRoot).verify(tool);
        if(!error.isEmpty())return fail(error,6);
        catalog.tools={tool};catalog.errors.clear();packagesRoot.clear();
    }
    for (const auto &error : catalog.errors)
        qWarning().noquote() << error;
    WorkbenchConfig config{dataRoot,packagesRoot,QCoreApplication::applicationDirPath()+"/ModynToolbox.WorkerHost.exe"};
    if(!isolated)config.startup=std::make_shared<StartupRegistration>(StartupTarget{QCoreApplication::applicationFilePath(),dataRoot,packagesRoot});
    Workbench window(catalog.tools,config);
    QPointer<Workbench> mainWindow=&window;
    if (!window.storageError().isEmpty())
        return fail(window.storageError(), 5);
    if(qEnvironmentVariableIsSet("MODYN_BACKGROUND_VALIDATION")){window.setWindowFlag(Qt::Tool);window.setAttribute(Qt::WA_ShowWithoutActivating);window.move(-20000,-20000);}
    window.show();
    instance.setActivationHandler([mainWindow]{if(mainWindow)restoreWorkbenchWindow(mainWindow);});
    if (parser.isSet("automation")) {
        const auto result = startAutomation(&window, parser.value("automation"));
        if (result)
            return result;
    }
    return app.exec();
}

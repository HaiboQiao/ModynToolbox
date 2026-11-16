#include "platform/windows/single_instance.h"
#include "platform/windows/startup_registration.h"
#include "ui/window/chrome_window.h"
#include "ui/theme/theme.h"
#include "ui/workbench.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <algorithm>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QtTest>
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>
namespace {
QString output(const QString &name){return qEnvironmentVariable("MODYN_SOURCE_ROOT")+"/.local/logs/DS1-"+name;}
QString read(const QString &name){QFile f(name);if(!f.open(QIODevice::ReadOnly))return {};return QString::fromUtf8(f.readAll());}
void write(const QString &name,const QByteArray &value){QFile f(name);if(f.open(QIODevice::WriteOnly|QIODevice::Truncate))f.write(value);}
StartupTarget target(const QString &root){return {QCoreApplication::applicationFilePath(),root+"/工作区 data "+QString(90,'d'),root+"/工具 tools "+QString(90,'t')};}
int child(int argc,char **argv){
    QCoreApplication app(argc,argv);const auto args=app.arguments();const auto scope=args.value(2),ready=args.value(3),activations=args.value(4);
    SingleInstance instance(scope);const auto result=instance.acquire(!args.contains("--silent"));
    if(result==SingleInstance::Result::Primary){
        write(ready,QByteArray::number(QCoreApplication::applicationPid()));
        if(args.contains("--block-broker"))QThread::sleep(6);
        // Delayed handler registration exercises an activation received during primary startup.
        QTimer::singleShot(250,&app,[&]{instance.setActivationHandler([&]{write(activations,QByteArray::number(read(activations).toInt()+1));});});
        QTimer::singleShot(12000,&app,&QCoreApplication::quit);return app.exec();
    }
    QTextStream(stdout)<<(result==SingleInstance::Result::Secondary?"secondary":instance.errorString())<<Qt::endl;
    return result==SingleInstance::Result::Secondary?0:2;
}
}
class DesktopStartupTests:public QObject {
    Q_OBJECT
private slots:
    void closeControlStates(){
        Theme::apply();ChromeWindow window;window.resize(900,300);window.setContentWidget(new QWidget);window.ensurePolished();
        auto *button=window.findChild<QToolButton*>("windowClose");QVERIFY(button);
        button->setAttribute(Qt::WA_UnderMouse,false);auto normal=button->grab().toImage();
        button->setAttribute(Qt::WA_UnderMouse,true);auto hover=button->grab().toImage();
        button->setDown(true);auto pressed=button->grab().toImage();
        QCOMPARE(hover.pixelColor(1,1).rgb(),Theme::closeHover.rgb());QCOMPARE(pressed.pixelColor(1,1).rgb(),Theme::closePressed.rgb());
        QVERIFY(normal.pixelColor(1,1).rgb()!=Theme::closeHover.rgb());
        int white=0;for(int y=hover.height()/2-8;y<hover.height()/2+8;++y)for(int x=hover.width()/2-8;x<hover.width()/2+8;++x){auto c=hover.pixelColor(x,y);if(c.red()>220&&c.green()>220&&c.blue()>220)++white;}QVERIFY(white>5);
        QImage strip(360,90,QImage::Format_RGB32);strip.fill(Qt::white);QPainter painter(&strip);int at=0;
        for(auto image:{normal,hover,pressed}){const auto box=QRect(at*120,25,110,60);painter.drawImage(QRect(box.x()+15,box.y(),76,68),image);++at;}
        painter.drawText(10,18,"Normal");painter.drawText(130,18,"Hover");painter.drawText(250,18,"Pressed");painter.end();QVERIFY(strip.save(output("close-states.png")));
        button->setDown(false);button->setAttribute(Qt::WA_UnderMouse,false);
    }
    void shortcutLifecycleAndArguments(){
        QTemporaryDir root;QVERIFY(root.isValid());const auto folder=root.path()+"/Startup 中文 空格";
        StartupRegistration startup(target(root.path()),folder);QVERIFY(!startup.status().exists);
        QVERIFY2(startup.setEnabled(true).isEmpty(),qPrintable(startup.status().error));const auto enabled=startup.status();
        QVERIFY(enabled.exists&&enabled.owned&&enabled.current);QVERIFY(enabled.arguments.size()>260);
        const auto command=("test.exe "+enabled.arguments).toStdWString();int count=0;auto **parts=CommandLineToArgvW(command.c_str(),&count);QVERIFY(parts);QStringList arguments;for(int i=1;i<count;++i)arguments<<QString::fromWCharArray(parts[i]);LocalFree(parts);
        QCOMPARE(arguments,QStringList({"--autostart","--data-root",QDir::toNativeSeparators(target(root.path()).dataRoot),"--packages-root",QDir::toNativeSeparators(target(root.path()).packagesRoot)}));
        QCOMPARE(enabled.workingDirectory,QDir::toNativeSeparators(QFileInfo(QCoreApplication::applicationFilePath()).absolutePath()));
        auto changedTarget=target(root.path());changedTarget.dataRoot=root.path()+"/another";StartupRegistration changed(changedTarget,folder);QVERIFY(changed.status().owned&&!changed.status().current);
        QVERIFY(changed.setEnabled(true).isEmpty());QVERIFY(changed.status().current);QVERIFY(!startup.status().current);
        StartupRegistration missing({root.path()+"/missing.exe",root.path(),root.path()},folder);QVERIFY(!missing.setEnabled(true).isEmpty());QVERIFY(changed.status().current);
        QVERIFY(changed.setEnabled(false).isEmpty());QVERIFY(!QFileInfo::exists(changed.linkPath()));QVERIFY(changed.setEnabled(false).isEmpty());
    }
    void shortcutFailurePreservesForeignFiles(){
        QTemporaryDir root;QVERIFY(root.isValid());const auto folder=root.path()+"/Startup";QVERIFY(QDir().mkpath(folder));
        StartupRegistration startup(target(root.path()),folder);write(startup.linkPath(),"foreign link content");
        QVERIFY(!startup.setEnabled(true).isEmpty());QVERIFY(!startup.setEnabled(false).isEmpty());QCOMPARE(read(startup.linkPath()),QString("foreign link content"));
        const auto blocked=root.path()+"/not-a-directory";write(blocked,"owned fixture");StartupRegistration denied(target(root.path()),blocked);QVERIFY(!denied.setEnabled(true).isEmpty());QCOMPARE(read(blocked),QString("owned fixture"));
    }
    void validForeignAndBrokenShortcutStayProtected(){
        QTemporaryDir root;QVERIFY(root.isValid());const auto program=root.path()+"/fixture.exe";write(program,"test fixture");
        StartupRegistration startup({program,root.path()+"/data",root.path()+"/tools"},root.path()+"/Startup");QVERIFY(startup.setEnabled(true).isEmpty());
        QVERIFY(QFile::remove(program));QVERIFY(startup.status().exists&&startup.status().owned);
        const auto co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);QVERIFY(SUCCEEDED(co));
        {
            Microsoft::WRL::ComPtr<IShellLinkW> link;QVERIFY(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))));
            Microsoft::WRL::ComPtr<IPersistFile> file;QVERIFY(SUCCEEDED(link.As(&file)));auto name=QDir::toNativeSeparators(startup.linkPath()).toStdWString();
            QVERIFY(SUCCEEDED(file->Load(name.c_str(),STGM_READWRITE)));QVERIFY(SUCCEEDED(link->SetDescription(L"Foreign user-managed link")));QVERIFY(SUCCEEDED(file->Save(name.c_str(),TRUE)));
        }
        CoUninitialize();QVERIFY(startup.status().exists&&!startup.status().owned);const auto before=read(startup.linkPath());
        QVERIFY(!startup.setEnabled(true).isEmpty());QVERIFY(!startup.setEnabled(false).isEmpty());QCOMPARE(read(startup.linkPath()),before);
    }
    void settingsCancelApplyAndDisable(){
        QTemporaryDir root;QVERIFY(root.isValid());auto startup=std::make_shared<StartupRegistration>(target(root.path()),root.path()+"/Startup");
        WorkbenchConfig config{root.path()+"/data",{},QCoreApplication::applicationDirPath()+"/ModynToolbox.WorkerHost.exe"};config.startup=startup;Workbench window({},config);
        auto *entry=window.findChild<QPushButton*>("settingsShortcut");QVERIFY(entry);
        auto change=[&](bool initial,bool checked,bool accept,bool capture){
            bool inspected=false;
            QTimer::singleShot(0,&window,[&]{
                auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
                auto *field=dialog->findChild<QCheckBox*>("preferenceStartup");auto *buttons=dialog->findChild<QDialogButtonBox*>();
                inspected=field&&buttons&&field->isEnabled()&&field->isChecked()==initial;
                if(field)field->setChecked(checked);if(capture)dialog->grab().save(output("settings.png"));
                if(buttons)buttons->button(accept?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();else dialog->reject();
            });
            QTimer::singleShot(2000,&window,[]{if(auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});
            entry->click();return inspected;
        };
        QVERIFY(change(false,true,false,false));QVERIFY(!startup->status().exists);
        QVERIFY(change(false,true,true,true));QVERIFY(startup->status().current);
        QVERIFY(change(true,false,true,false));QVERIFY(!startup->status().exists);
    }
    void instanceActivationSilentAndRecovery(){
        QTemporaryDir root;QVERIFY(root.isValid());const auto scope="ds1-"+QUuid::createUuid().toString();const auto ready=root.path()+"/ready",counter=root.path()+"/activations";
        QProcess primary;primary.start(QCoreApplication::applicationFilePath(),{"--instance-child",scope,ready,counter});QVERIFY(primary.waitForStarted());QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(ready),5000);
        auto duplicate=[&](bool silent){QProcess other;QStringList args{"--instance-child",scope,root.path()+"/must-not-write",counter};if(silent)args<<"--silent";other.start(QCoreApplication::applicationFilePath(),args);if(!other.waitForFinished(5000))return false;return other.exitStatus()==QProcess::NormalExit&&other.exitCode()==0&&other.readAllStandardOutput().contains("secondary");};
        QVERIFY(duplicate(false));QTRY_COMPARE_WITH_TIMEOUT(read(counter).toInt(),1,3000);
        QVERIFY(duplicate(true));QTest::qWait(100);QCOMPARE(read(counter).toInt(),1);
        QVERIFY(duplicate(false));QTRY_COMPARE_WITH_TIMEOUT(read(counter).toInt(),2,3000);QVERIFY(!QFileInfo::exists(root.path()+"/must-not-write"));
        primary.kill();QVERIFY(primary.waitForFinished(5000));QVERIFY(QFile::remove(ready));
        QProcess restarted;restarted.start(QCoreApplication::applicationFilePath(),{"--instance-child",scope,ready,counter});QVERIFY(restarted.waitForStarted());QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(ready),5000);restarted.kill();QVERIFY(restarted.waitForFinished(5000));
    }
    void unresponsivePrimaryDoesNotOpenAnother(){
        QTemporaryDir root;QVERIFY(root.isValid());const auto scope="ds1-busy-"+QUuid::createUuid().toString();const auto ready=root.path()+"/ready";
        QProcess primary;primary.start(QCoreApplication::applicationFilePath(),{"--instance-child",scope,ready,root.path()+"/counter","--block-broker"});QVERIFY(primary.waitForStarted());QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(ready),5000);
        QProcess duplicate;duplicate.start(QCoreApplication::applicationFilePath(),{"--instance-child",scope,root.path()+"/unexpected-primary",root.path()+"/counter"});QVERIFY(duplicate.waitForFinished(5500));QCOMPARE(duplicate.exitCode(),2);QVERIFY(!QFileInfo::exists(root.path()+"/unexpected-primary"));QVERIFY(primary.state()!=QProcess::NotRunning);
        primary.kill();QVERIFY(primary.waitForFinished(5000));
    }
    void simultaneousLaunchesHaveOnePrimary(){
        QTemporaryDir root;QVERIFY(root.isValid());const auto scope="ds1-race-"+QUuid::createUuid().toString();QVector<std::shared_ptr<QProcess>> children;
        for(int i=0;i<4;++i){auto child=std::make_shared<QProcess>();child->start(QCoreApplication::applicationFilePath(),{"--instance-child",scope,root.path()+"/primary-"+QString::number(i),root.path()+"/counter","--silent"});QVERIFY(child->waitForStarted());children.append(child);}
        QTRY_COMPARE_WITH_TIMEOUT(QDir(root.path()).entryList({"primary-*"},QDir::Files).size(),1,5000);
        QTRY_VERIFY_WITH_TIMEOUT(std::count_if(children.begin(),children.end(),[](auto p){return p->state()==QProcess::NotRunning;})==3,5000);
        for(auto child:children){if(child->state()!=QProcess::NotRunning){child->kill();QVERIFY(child->waitForFinished(5000));}else QCOMPARE(child->exitCode(),0);}
        QCOMPARE(QDir(root.path()).entryList({"primary-*"},QDir::Files).size(),1);
    }
    void logicalWindowRestorePreservesMaximized(){
        qputenv("MODYN_BACKGROUND_VALIDATION","1");ChromeWindow window;window.setAttribute(Qt::WA_ShowWithoutActivating);window.setWindowState(Qt::WindowMaximized|Qt::WindowMinimized);
        restoreWorkbenchWindow(&window);QVERIFY(!window.isMinimized());QVERIFY(window.isMaximized());window.hide();
    }
};
int main(int argc,char **argv){
    if(argc>1&&QString::fromLocal8Bit(argv[1])=="--instance-child")return child(argc,argv);
    QApplication app(argc,argv);
    if(QGuiApplication::platformName()=="offscreen"){
        const auto fonts=qEnvironmentVariable("WINDIR")+"/Fonts/";
        QFontDatabase::addApplicationFont(fonts+"segoeui.ttf");QFontDatabase::addApplicationFont(fonts+"msyh.ttc");
    }
    DesktopStartupTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "test_desktop_startup.moc"

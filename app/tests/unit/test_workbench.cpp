#include "ui/workbench.h"
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QStackedWidget>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSlider>
#include <QTableWidget>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QToolButton>
#include <QLabel>
#include <QUrl>
#include <QtTest>
QVector<ToolDefinition> testTools() {
    return {{"alpha","Alpha","1.0.0","text",""},{"beta","Beta","1.0.0","files",""}};
}
class WorkbenchTests : public QObject {
    Q_OBJECT
    QVector<ToolDefinition> previewTools(const QString &root) {
        const auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");
        for (const auto &id : {"image","json","rename"}) {
            const auto destination=root+"/"+id+"/0.2.0";
            if (!QDir().mkpath(destination)) return {};
            const auto folder=source+"/tools/"+id+"/integration/";
            if (!QFile::copy(folder+"manifest.json",destination+"/manifest.json")) return {};
            if (!QFile::copy(folder+"ui.json",destination+"/ui.json")) return {};
        }
        return readCatalog(root).tools;
    }
private slots:
    void initTestCase() {
        const auto fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
        QVERIFY(QFontDatabase::addApplicationFont(fonts + "segoeui.ttf") >= 0);
        QVERIFY(QFontDatabase::addApplicationFont(fonts + "msyh.ttc") >= 0);
    }
    void toolsAndSearch() {
        Workbench window(testTools());
        window.show();
        auto *search = window.findChild<QLineEdit *>("toolSearch");
        auto *list = window.findChild<QListWidget *>("toolNavigation");
        QCOMPARE(list->count(), 2);
        QCOMPARE(search->placeholderText(), QStringLiteral("搜索工具"));
        search->setText("Beta");
        QVERIFY(list->item(0)->isHidden());
        QVERIFY(!list->item(1)->isHidden());
        search->clear();
        window.editor(1)->setPlainText("alpha");
        list->setCurrentRow(1);
        QCOMPARE(window.sessionCount(), 2);
        list->setCurrentRow(0);
        QCOMPARE(window.editor(1)->toPlainText(), QString("alpha"));
        QVERIFY(window.findChild<QComboBox *>("sessionSelector")->isHidden());
    }
    void sessionNavigation() {
        Workbench window(testTools());
        window.show();
        auto *list = window.findChild<QListWidget *>("toolNavigation");
        auto *selector = window.findChild<QComboBox *>("sessionSelector");
        window.editor(1)->setPlainText("first");
        window.addSession();
        window.editor(2)->setPlainText("second");
        QVERIFY(!selector->isHidden());
        selector->setCurrentIndex(0);
        QCOMPARE(window.editor(1)->toPlainText(), QString("first"));
        window.detachSession(1);
        auto *stack = window.findChild<QStackedWidget *>();
        QCOMPARE(stack->currentWidget()->objectName(), QString("floatingNotice"));
        list->setCurrentRow(1);
        window.attachSession(1);
        QCOMPARE(list->currentRow(), 0);
        QCOMPARE(stack->currentWidget(), window.sessionView(1));
        QCOMPARE(window.editor(2)->toPlainText(), QString("second"));
        auto *search = window.findChild<QLineEdit *>("toolSearch");
        search->setText("absent");
        QVERIFY(list->item(0)->isHidden());
        QVERIFY(list->item(1)->isHidden());
        QCOMPARE(stack->currentWidget(), window.sessionView(1));
    }
    void independentSessions() {
        Workbench window(testTools());
        window.addSession();
        QCOMPARE(window.sessionCount(), 2);
        window.editor(1)->setPlainText("alpha");
        window.editor(2)->setPlainText("beta");
        QCOMPARE(window.editor(1)->toPlainText(), QString("alpha"));
        QCOMPARE(window.editor(2)->toPlainText(), QString("beta"));
    }
    void detachAndReturn() {
        Workbench window(testTools());
        window.addSession();
        window.show();
        auto *original = window.sessionView(1);
        window.editor(1)->setPlainText("preserved");
        window.detachSession(1);
        QVERIFY(window.isDetached(1));
        QVERIFY(window.editor(1)->isVisible());
        QCOMPARE(window.sessionView(1), original);
        window.attachSession(1);
        QVERIFY(!window.isDetached(1));
        QCOMPARE(window.editor(1)->toPlainText(), QString("preserved"));
        window.detachSession(1);
        window.sessionView(1)->window()->close();
        QVERIFY(!window.isDetached(1));
        QCOMPARE(window.sessionCount(), 2);
    }
    void fileAndTextViewsPreserveState() {
        QTemporaryDir root;
        const auto tools=previewTools(root.path());
        QCOMPARE(tools.size(),3);
        Workbench window(tools);
        window.show();
        auto *quality=window.sessionView(1)->findChild<QSlider *>("quality");
        QVERIFY(quality);
        quality->setValue(67);
        QFile file(root.path()+"/sample.png");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("input-selection-fixture");
        file.close();
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(file.fileName())});
        auto *drop=window.sessionView(1)->findChild<QWidget *>("fileDropArea");
        QDragEnterEvent enter(QPoint(10,10),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(drop,&enter);
        QVERIFY(enter.isAccepted());
        QDropEvent event(QPointF(10,10),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(drop,&event);
        auto *table=window.sessionView(1)->findChild<QTableWidget *>("inputFiles");
        QCOMPARE(table->rowCount(),1);
        window.detachSession(1);
        window.findChild<QListWidget *>("toolNavigation")->setCurrentRow(1);
        QVERIFY(window.editor(2));
        window.editor(2)->setPlainText("{\"local\":true}");
        auto *floating=window.sessionView(1)->window();
        floating->showMaximized();
        window.findChild<QListWidget *>("toolNavigation")->setCurrentRow(0);
        QVERIFY(floating->isMaximized());
        window.showMinimized();
        window.attachSession(1);
        QVERIFY(!window.isMinimized());
        QCOMPARE(quality->value(),67);
        QCOMPARE(table->rowCount(),1);
        window.addSession();
        QCOMPARE(window.sessionView(3)->findChild<QSlider *>("quality")->value(),80);
        QCOMPARE(window.sessionView(3)->findChild<QTableWidget *>("inputFiles")->rowCount(),0);
    }
    void brandAndWindowControls() {
        Workbench window(testTools());
        window.show();
        QCOMPARE(window.windowTitle(),QStringLiteral("ModynToolbox"));
        QCOMPARE(window.findChild<QLabel *>("appTitle")->text(),QStringLiteral("ModynToolbox"));
        QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
        window.maximizeButton()->click();
        QVERIFY(window.isMaximized());
        window.maximizeButton()->click();
        QVERIFY(!window.isMaximized());
    }
    void savePreview() {
        QTemporaryDir root;
        const auto tools=previewTools(root.path());
        QCOMPARE(tools.size(),3);
        Workbench window(tools);
        window.show();
        QTest::qWait(60);
        const auto path=qEnvironmentVariable("MODYN_PREVIEW_PATH");
        if (!path.isEmpty()) {
            QVERIFY(window.grab().save(path));
            window.resize(920,600);
            QTest::qWait(30);
            QVERIFY(window.grab().save(path+".compact.png"));
            window.detachSession(1);
            QTest::qWait(30);
            QVERIFY(window.sessionView(1)->window()->grab().save(path+".floating.png"));
            window.attachSession(1);
        }
    }
    void multipleIndependentWindows() {
        Workbench window(testTools());
        window.show();
        window.editor(1)->setPlainText("alpha");
        window.findChild<QListWidget *>("toolNavigation")->setCurrentRow(1);
        window.editor(2)->setPlainText("beta");
        window.detachSession(1);
        window.detachSession(2);
        QVERIFY(window.isDetached(1));
        QVERIFY(window.isDetached(2));
        window.sessionView(1)->window()->close();
        QVERIFY(!window.isDetached(1));
        QVERIFY(window.isDetached(2));
        QCOMPARE(window.editor(1)->toPlainText(),QString("alpha"));
        window.sessionView(2)->window()->close();
        QVERIFY(!window.isDetached(2));
        QCOMPARE(window.editor(2)->toPlainText(),QString("beta"));
        QCOMPARE(window.sessionCount(),2);
    }
    void repeatedMoves() {
        Workbench window(testTools());
        window.addSession();
        for (int i=0;i<50;++i) {
            window.detachSession(1);
            window.attachSession(1);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        QCOMPARE(window.sessionCount(), 2);
        QVERIFY(!window.isDetached(1));
    }
};
QTEST_MAIN(WorkbenchTests)
#include "test_workbench.moc"

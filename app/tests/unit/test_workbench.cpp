#include "ui/workbench.h"
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QStackedWidget>
#include <QFontDatabase>
#include <QtTest>
QVector<ToolDefinition> testTools() {
    return {{"alpha","Alpha","1.0.0","text",""},{"beta","Beta","1.0.0","files",""}};
}
class WorkbenchTests : public QObject {
    Q_OBJECT
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
    void savePreview() {
        Workbench window({
            {"json",QStringLiteral("JSON 工具"),"0.1.0",QStringLiteral("接入预览，JSON 校验与格式化功能将在工具阶段实现。"),""},
            {"image",QStringLiteral("图片压缩"),"0.1.0",QStringLiteral("接入预览"),""},
            {"rename",QStringLiteral("批量重命名"),"0.1.0",QStringLiteral("接入预览"),""}
        });
        window.show();
        QTest::qWait(30);
        const auto path = qEnvironmentVariable("GRAPHITE_PREVIEW_PATH");
        if (!path.isEmpty()) QVERIFY(window.grab().save(path));
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

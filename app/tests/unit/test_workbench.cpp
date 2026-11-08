#include "ui/workbench.h"
#include <QPlainTextEdit>
#include <QtTest>
class WorkbenchTests : public QObject {
    Q_OBJECT
private slots:
    void independentSessions() {
        Workbench window;
        QCOMPARE(window.sessionCount(), 2);
        window.editor(1)->setPlainText("alpha");
        window.editor(2)->setPlainText("beta");
        QCOMPARE(window.editor(1)->toPlainText(), QString("alpha"));
        QCOMPARE(window.editor(2)->toPlainText(), QString("beta"));
    }
    void detachAndReturn() {
        Workbench window;
        auto *original = window.sessionView(1);
        window.editor(1)->setPlainText("preserved");
        window.detachSession(1);
        QVERIFY(window.isDetached(1));
        QCOMPARE(window.sessionView(1), original);
        window.attachSession(1);
        QVERIFY(!window.isDetached(1));
        QCOMPARE(window.editor(1)->toPlainText(), QString("preserved"));
        window.detachSession(1);
        window.sessionView(1)->window()->close();
        QVERIFY(!window.isDetached(1));
        QCOMPARE(window.sessionCount(), 2);
    }
    void repeatedMoves() {
        Workbench window;
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

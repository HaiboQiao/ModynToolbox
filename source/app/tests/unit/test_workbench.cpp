#include "ui/workbench.h"
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QTreeWidget>
#include "ui/components/tool_navigation.h"
#include <QComboBox>
#include <QStackedWidget>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSlider>
#include <QTableWidget>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QToolButton>
#include <QLabel>
#include <QSplitter>
#include <QPushButton>
#include <QUrl>
#include <QtTest>
QVector<ToolDefinition> testTools() {
    return {{"alpha", "Alpha", "1.0.0", "text", ""}, {"beta", "Beta", "1.0.0", "files", ""}};
}
class WorkbenchTests : public QObject {
    Q_OBJECT
    void choose(QTreeWidget *tree, int row) {
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tree->visualItemRect(tree->topLevelItem(row)).center());
    }
    void mouse(QTreeWidget *tree, QEvent::Type type, const QPoint &global) {
        const auto local = tree->viewport()->mapFromGlobal(global);
        QMouseEvent event(type, QPointF(local), QPointF(global),
                          type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                          type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &event);
    }
    QVector<ToolDefinition> previewTools(const QString &root) {
        const auto source = qEnvironmentVariable("MODYN_SOURCE_ROOT");
        for (const auto &id : {"image", "json", "rename"}) {
            const auto destination = root + "/" + id + "/0.2.0";
            if (!QDir().mkpath(destination))
                return {};
            const auto folder = source + "/tools/" + id + "/integration/";
            QFile source(folder + "manifest.json");
            if (!source.open(QIODevice::ReadOnly))
                return {};
            auto object = QJsonDocument::fromJson(source.readAll()).object();
            object.remove("protocol_version");
            object["package_version"] = "0.2.0";
            QFile output(destination + "/manifest.json");
            if (!output.open(QIODevice::WriteOnly))
                return {};
            output.write(QJsonDocument(object).toJson());
            output.close();
            if (!QFile::copy(folder + "ui.json", destination + "/ui.json"))
                return {};
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
        auto *list = window.findChild<QTreeWidget *>("toolNavigation");
        QCOMPARE(list->topLevelItemCount(), 2);
        QCOMPARE(search->placeholderText(), QStringLiteral("搜索工具"));
        search->setText("Beta");
        QVERIFY(list->topLevelItem(0)->isHidden());
        QVERIFY(!list->topLevelItem(1)->isHidden());
        search->clear();
        window.editor(1)->setPlainText("alpha");
        choose(list, 1);
        QCOMPARE(window.sessionCount(), 2);
        choose(list, 0);
        QCOMPARE(window.editor(1)->toPlainText(), QString("alpha"));
        QVERIFY(!window.findChild<QComboBox *>("sessionSelector"));
    }
    void sessionNavigation() {
        Workbench window(testTools());
        window.show();
        auto *list = window.findChild<QTreeWidget *>("toolNavigation");
        window.editor(1)->setPlainText("first");
        window.addSession();
        window.editor(2)->setPlainText("second");
        QCOMPARE(list->topLevelItem(0)->childCount(), 2);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          list->visualItemRect(list->topLevelItem(0)->child(0)).center());
        QCOMPARE(window.editor(1)->toPlainText(), QString("first"));
        window.detachSession(1);
        auto *stack = window.findChild<QStackedWidget *>("primaryWorkspace");
        QCOMPARE(stack->currentWidget()->objectName(), QString("floatingNotice"));
        choose(list, 1);
        window.attachSession(1);
        QCOMPARE(list->currentItem()->data(0, ToolNavigation::SessionRole).toInt(), 1);
        QCOMPARE(stack->currentWidget(), window.sessionView(1));
        QCOMPARE(window.editor(2)->toPlainText(), QString("second"));
        auto *search = window.findChild<QLineEdit *>("toolSearch");
        search->setText("absent");
        QVERIFY(list->topLevelItem(0)->isHidden());
        QVERIFY(list->topLevelItem(1)->isHidden());
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
        const auto tools = previewTools(root.path());
        QCOMPARE(tools.size(), 3);
        Workbench window(tools);
        window.show();
        auto *quality = window.sessionView(1)->findChild<QSlider *>("quality");
        QVERIFY(quality);
        quality->setValue(67);
        QFile file(root.path() + "/sample.png");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("input-selection-fixture");
        file.close();
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(file.fileName())});
        auto *drop = window.sessionView(1)->findChild<QWidget *>("fileDropArea");
        QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(drop, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent event(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(drop, &event);
        auto *table = window.sessionView(1)->findChild<QTableWidget *>("inputFiles");
        QTRY_COMPARE(table->rowCount(), 1);
        window.detachSession(1);
        choose(window.findChild<QTreeWidget *>("toolNavigation"), 1);
        QVERIFY(window.editor(2));
        window.editor(2)->setPlainText("{\"local\":true}");
        auto *floating = window.sessionView(1)->window();
        floating->showMaximized();
        choose(window.findChild<QTreeWidget *>("toolNavigation"), 0);
        QVERIFY(floating->isMaximized());
        window.showMinimized();
        window.attachSession(1);
        QVERIFY(!window.isMinimized());
        QCOMPARE(quality->value(), 67);
        QTRY_COMPARE(table->rowCount(), 1);
        window.addSession();
        QCOMPARE(window.sessionView(3)->findChild<QSlider *>("quality")->value(), 80);
        QCOMPARE(window.sessionView(3)->findChild<QTableWidget *>("inputFiles")->rowCount(), 0);
    }
    void brandAndWindowControls() {
        Workbench window(testTools());
        window.show();
        QCOMPARE(window.windowTitle(), QStringLiteral("ModynToolbox"));
        QCOMPARE(window.findChild<QLabel *>("appTitle")->text(), QStringLiteral("ModynToolbox"));
        QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint));
        window.maximizeButton()->click();
        QVERIFY(window.isMaximized());
        window.maximizeButton()->click();
        QVERIFY(!window.isMaximized());
    }
    void savePreview() {
        QTemporaryDir root;
        const auto tools = previewTools(root.path());
        QCOMPARE(tools.size(), 3);
        Workbench window(tools);
        window.show();
        QTest::qWait(60);
        const auto path = qEnvironmentVariable("MODYN_PREVIEW_PATH");
        if (!path.isEmpty()) {
            QVERIFY(window.grab().save(path));
            window.resize(920, 600);
            QTest::qWait(30);
            QVERIFY(window.grab().save(path + ".compact.png"));
            window.detachSession(1);
            QTest::qWait(30);
            QVERIFY(window.sessionView(1)->window()->grab().save(path + ".floating.png"));
            window.attachSession(1);
        }
    }
    void multipleIndependentWindows() {
        Workbench window(testTools());
        window.show();
        window.editor(1)->setPlainText("alpha");
        choose(window.findChild<QTreeWidget *>("toolNavigation"), 1);
        window.editor(2)->setPlainText("beta");
        window.detachSession(1);
        window.detachSession(2);
        QVERIFY(window.isDetached(1));
        QVERIFY(window.isDetached(2));
        window.sessionView(1)->window()->close();
        QVERIFY(!window.isDetached(1));
        QVERIFY(window.isDetached(2));
        QCOMPARE(window.editor(1)->toPlainText(), QString("alpha"));
        window.sessionView(2)->window()->close();
        QVERIFY(!window.isDetached(2));
        QCOMPARE(window.editor(2)->toPlainText(), QString("beta"));
        QCOMPARE(window.sessionCount(), 2);
    }
    void categoryAndKeywordSearch() {
        QVector<ToolDefinition> tools;
        for (int index = 0; index < 9; ++index) {
            ToolDefinition tool;
            tool.id = "tool-" + QString::number(index);
            tool.name = "Tool " + QString::number(index);
            tool.version = "1.0.0";
            tool.category = index % 2 ? QStringLiteral("图片") : QStringLiteral("开发");
            if (index == 0)
                tool.keywords = QStringList{"needle"};
            tools.append(tool);
        }
        Workbench window(tools);
        window.show();
        auto *search = window.findChild<QLineEdit *>("toolSearch");
        auto *list = window.findChild<QTreeWidget *>("toolNavigation");
        auto *category = window.findChild<QComboBox *>("toolCategory");
        QVERIFY(category->isVisible());
        search->setText("needle");
        QVERIFY(!list->topLevelItem(0)->isHidden());
        QVERIFY(list->topLevelItem(1)->isHidden());
        search->clear();
        category->setCurrentIndex(category->findData(QStringLiteral("图片")));
        QVERIFY(list->topLevelItem(0)->isHidden());
        QVERIFY(!list->topLevelItem(1)->isHidden());
    }
    void balancedPairAndEmptyWorkspace() {
        Workbench window(testTools());
        window.show();
        window.addSession();
        window.pairSession(1);
        QTest::qWait(30);
        const auto panes = window.findChild<QSplitter *>("workspaceSplitter")->sizes();
        QCOMPARE(panes.size(), 2);
        QVERIFY(qAbs(panes[0] - panes[1]) <= 2);
        QVERIFY(!window.findChild<QLabel *>("pairedHeading"));
        window.resize(920, 600);
        QTest::qWait(20);
        QVERIFY(!window.findChild<QLabel *>("pairedHeading"));
        window.closeSession(2, true);
        window.closeSession(1, true);
        QCOMPARE(window.sessionCount(), 0);
        QVERIFY(window.findChild<QLabel *>("workspaceNoticeText")
                    ->text()
                    .contains(QStringLiteral("尚未打开会话")));
        QVERIFY(window.findChild<QPushButton *>("returnFloatingButton")->isHidden());
    }
    void sidebarCreatesBranchesAndOwnsSessionControls() {
        Workbench window(testTools());
        window.show();
        QTest::qWait(20);
        auto *tree = window.findChild<QTreeWidget *>("toolNavigation");
        window.editor(1)->setPlainText("preserved");
        QCOMPARE(tree->topLevelItem(0)->childCount(), 0);
        const auto row = tree->visualItemRect(tree->topLevelItem(0));
        QTest::mouseClick(
            tree->viewport(), Qt::LeftButton, Qt::NoModifier,
            QPoint(tree->columnViewportPosition(1) + tree->columnWidth(1) / 2, row.center().y()));
        QCOMPARE(window.sessionCount(), 2);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 2);
        QVERIFY(tree->topLevelItem(0)->isExpanded());
        QCOMPARE(tree->topLevelItem(0)->child(0)->text(0), QStringLiteral("默认会话"));
        QCOMPARE(window.editor(1)->toPlainText(), QString("preserved"));
        QVERIFY(window.editor(2)->toPlainText().isEmpty());
        QVERIFY(!window.findChild<QComboBox *>("sessionSelector"));
        QVERIFY(!window.sessionView(2)->findChild<QPushButton *>("sessionMenu"));
        QVERIFY(!window.sessionView(2)->findChild<QPushButton *>("locationAction"));
        QVERIFY(!window.sessionView(2)->findChild<QLabel *>("heading"));
        QTest::keyClick(tree, Qt::Key_N, Qt::ControlModifier);
        QCOMPARE(window.sessionCount(), 3);
        window.closeSession(3, true);
        window.closeSession(2, true);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 0);
        const auto *title = window.findChild<QLabel *>("appTitle");
        const auto *caption = window.findChild<QLabel *>("caption");
        QVERIFY(caption->mapTo(&window, QPoint()).y() > title->mapTo(&window, QPoint()).y());
        QVERIFY(caption->text().contains("v" + QStringLiteral(MODYN_VERSION)));
    }
    void sidebarDragDetachesAndCancelsWithoutLosingInput() {
        Workbench window(testTools());
        window.show();
        window.addSession();
        QTest::qWait(20);
        window.editor(2)->setPlainText("drag data");
        auto *tree = window.findChild<QTreeWidget *>("toolNavigation");
        auto start = [&] {
            return tree->viewport()->mapToGlobal(
                tree->visualItemRect(tree->topLevelItem(0)->child(1)).center());
        };
        const auto inside = window.mapToGlobal(QPoint(window.width() - 80, 180));
        const auto outside = window.mapToGlobal(QPoint(window.width() + 50, 180));
        mouse(tree, QEvent::MouseButtonPress, start());
        mouse(tree, QEvent::MouseMove, inside);
        mouse(tree, QEvent::MouseButtonRelease, inside);
        QVERIFY(!window.isDetached(2));
        mouse(tree, QEvent::MouseButtonPress, start());
        mouse(tree, QEvent::MouseMove, outside);
        QTest::keyClick(tree, Qt::Key_Escape);
        mouse(tree, QEvent::MouseButtonRelease, outside);
        QVERIFY(!window.isDetached(2));
        mouse(tree, QEvent::MouseButtonPress, start());
        mouse(tree, QEvent::MouseMove, outside);
        mouse(tree, QEvent::MouseButtonRelease, outside);
        QVERIFY(window.isDetached(2));
        QCOMPARE(window.editor(2)->toPlainText(), QString("drag data"));
        QVERIFY(tree->topLevelItem(0)->child(1)->text(0).contains(QStringLiteral("↗")));
        window.sessionView(2)->window()->close();
        QVERIFY(!window.isDetached(2));
        const auto parent =
            tree->viewport()->mapToGlobal(tree->visualItemRect(tree->topLevelItem(0)).center());
        mouse(tree, QEvent::MouseButtonPress, parent);
        mouse(tree, QEvent::MouseMove, outside);
        mouse(tree, QEvent::MouseButtonRelease, outside);
        QVERIFY(window.isDetached(2));
        QCOMPARE(window.sessionCount(), 2);
    }
    void repeatedMoves() {
        Workbench window(testTools());
        window.addSession();
        for (int i = 0; i < 50; ++i) {
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

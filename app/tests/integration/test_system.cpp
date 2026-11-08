#include "storage/state_repository.h"
#include "storage/data_transfer.h"
#include "catalog/package_manager.h"
#include "core/files.h"
#include "ui/workbench.h"
#include "ui/components/tool_panel.h"
#include "jobs/scheduler.h"
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QTableWidget>
#include <QSplitter>
#include <QLabel>
#include "ui/theme/theme.h"
#include <QClipboard>
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
class SystemTests : public QObject {
    Q_OBJECT
    QString package(const QString &id) {
        return Files::readObject(qEnvironmentVariable("MODYN_SOURCE_ROOT") + "/.local/tools/" + id +
                                     "/latest.json",
                                 4096)
            .value("package")
            .toString();
    }
    void write(const QString &path, const QByteArray &value) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QCOMPARE(f.write(value), value.size());
    }
  private slots:
    void sqliteStateAndConsistentSnapshot() {
        QTemporaryDir root;
        const auto data = root.path() + "/data";
        StateRepository repository(data);
        QVERIFY2(repository.open(), qPrintable(repository.error()));
        QVERIFY(repository.save({{"proof", "saved"}}));
        write(data + "/tools/example/settings.json", "private-tool-state");
        const auto snapshot = root.path() + "/snapshot";
        QVERIFY2(DataTransfer::exportSnapshot(data, snapshot).isEmpty(), "export failed");
        QVERIFY(repository.save({{"proof", "changed"}}));
        repository.close();
        QCOMPARE(DataTransfer::restoreSnapshot(data, snapshot), QString());
        QVERIFY(repository.open());
        QCOMPARE(repository.state().value("proof").toString(), QString("saved"));
        repository.close();
        QVERIFY(
            !QDir(data + "/host/backups").entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }
    void corruptedSnapshotRejectedBeforeChanges() {
        QTemporaryDir root;
        const auto data = root.path() + "/data";
        StateRepository repository(data);
        QVERIFY(repository.open());
        QVERIFY(repository.save({{"value", 1}}));
        repository.close();
        const auto snapshot = root.path() + "/snapshot";
        QCOMPARE(DataTransfer::exportSnapshot(data, snapshot), QString());
        write(snapshot + "/extra.txt", "not-indexed");
        QVERIFY(!DataTransfer::restoreSnapshot(data, snapshot).isEmpty());
        QFile::remove(snapshot + "/extra.txt");
        write(snapshot + "/host/state.sqlite", "damaged");
        QVERIFY(!DataTransfer::restoreSnapshot(data, snapshot).isEmpty());
        QVERIFY(repository.open());
        QCOMPARE(repository.state().value("value").toInt(), 1);
    }
    void interruptedMigrationRollback() {
        QTemporaryDir root;
        const auto data = root.path();
        write(data + "/host/backups/test/original.txt", "before");
        QVERIFY(Files::writeObject(
            data + "/host/migration.json",
            {{"status", "moving"},
             {"moves", QJsonArray{QJsonObject{{"from", "original.txt"},
                                              {"to", "host/backups/test/original.txt"}}}}}));
        QCOMPARE(DataTransfer::recover(data), QString());
        QVERIFY(QFile::exists(data + "/original.txt"));
        QVERIFY(!QFile::exists(data + "/host/migration.json"));
    }
    void futureDatabaseRefused() {
        QTemporaryDir root;
        StateRepository repository(root.path());
        QVERIFY(repository.open());
        repository.close();
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "future");
            db.setDatabaseName(root.path() + "/host/state.sqlite");
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("PRAGMA user_version=99"));
            db.close();
        }
        QSqlDatabase::removeDatabase("future");
        QVERIFY(!repository.open());
        QVERIFY(!repository.save({{"must_not_write", true}}));
        QVERIFY(repository.state().isEmpty());
    }
    void packageInstallVerifyRemove() {
        QTemporaryDir root;
        PackageManager manager(root.path());
        QCOMPARE(manager.install(package("json")), QString());
        auto versions = manager.versions();
        QCOMPARE(versions.size(), 1);
        QVERIFY(!manager.install(package("json")).isEmpty());
        QVERIFY(!manager.remove(versions.first(), true).isEmpty());
        const auto path = versions.first().packageRoot;
        write(path + "/backend/adapter.py", "tampered");
        QVERIFY(!manager.verify(versions.first()).isEmpty());
        QCOMPARE(manager.remove(versions.first(), false), QString());
        QVERIFY(manager.versions().isEmpty());
    }
    void normalUiWorkerAndVersionPinning() {
        QTemporaryDir root;
        const auto packages = root.path() + "/tools";
        PackageManager manager(packages);
        QCOMPARE(manager.install(package("json")), QString());
        const WorkbenchConfig config{root.path() + "/data", packages,
                                     QCoreApplication::applicationDirPath() +
                                         "/ModynToolbox.WorkerHost.exe"};
        {
            Workbench window(readCatalog(packages).tools, config);
            window.show();
            QVERIFY(window.storageError().isEmpty());
            const int id = window.open("json");
            QVERIFY(id > 0);
            window.editor(id)->setPlainText("{\"precise\":123456789012345678901234567890}");
            window.setDraft(id, true);
            QSignalSpy finished(window.jobScheduler(), &Scheduler::finished);
            window.panel(id)->trigger("format");
            window.detachSession(id);
            QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 30000);
            QCOMPARE(finished.first().at(4).toString(), QString("succeeded"));
            QVERIFY(window.panel(id)->resultText().contains("123456789012345678901234567890"));
            window.attachSession(id);
            const int other = window.open("json", true);
            window.pairSession(id);
            QCOMPARE(window.sessionCount(), 2);
            QVERIFY(!window.removePackage("json", readCatalog(packages).tools.first().version)
                         .isEmpty());
            QVERIFY(window.saveState());
            Q_UNUSED(other);
        }
        {
            Workbench window(readCatalog(packages).tools, config);
            QCOMPARE(window.sessionCount(), 2);
            const auto saved = window.savedState();
            bool found = false;
            for (const auto &s : saved.value("sessions").toArray())
                found |= s.toObject()
                             .value("snapshot")
                             .toObject()
                             .value("text")
                             .toString()
                             .contains("precise");
            QVERIFY(found);
            QVERIFY(!window.jobScheduler()->busy());
        }
    }
    void largeTextClipboardAndFullProcessing() {
        QTemporaryDir root;
        const auto packages = root.path() + "/tools";
        PackageManager manager(packages);
        QCOMPARE(manager.install(package("json")), QString());
        Workbench window(readCatalog(packages).tools,
                         {root.path() + "/data", packages,
                          QCoreApplication::applicationDirPath() + "/ModynToolbox.WorkerHost.exe"});
        window.show();
        const int id = window.open("json");
        const auto text = QString("\"") + QString(5 * 1024 * 1024 - 2, 'x') + "\"";
        QApplication::clipboard()->setText(text);
        window.editor(id)->setFocus();
        QTest::keyClick(window.editor(id), Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(window.panel(id)->input().value("text").toString(), text);
        QVERIFY(window.editor(id)->isReadOnly());
        QVERIFY(window.editor(id)->toPlainText().size() <= 32768);
        QSignalSpy finished(window.jobScheduler(), &Scheduler::finished);
        window.panel(id)->trigger("format");
        Theme::setPreferences(13, false);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 30000);
        QCOMPARE(finished.first().at(4).toString(), QString("succeeded"));
        QCOMPARE(window.panel(id)->resultText(), text);
        QVERIFY(window.sessionView(id)
                    ->findChild<QPlainTextEdit *>("textResult")
                    ->toPlainText()
                    .size() <= 32768);
    }
    void preferencesNamesAndNavigationRestore() {
        QTemporaryDir root;
        const auto packages = root.path() + "/tools";
        PackageManager manager(packages);
        QCOMPARE(manager.install(package("json")), QString());
        const WorkbenchConfig config{root.path() + "/data", packages,
                                     QCoreApplication::applicationDirPath() +
                                         "/ModynToolbox.WorkerHost.exe"};
        {
            Workbench window(readCatalog(packages).tools, config);
            window.show();
            QTest::qWait(20);
            auto *manage = window.findChild<QPushButton *>("managementButton");
            QVERIFY(manage);
            QTimer::singleShot(0, &window, [] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                if (!dialog)
                    return;
                dialog->findChild<QSpinBox *>("preferenceFont")->setValue(13);
                dialog->findChild<QSpinBox *>("preferenceBatch")->setValue(2);
                dialog->findChild<QCheckBox *>("preferenceDraft")->setChecked(true);
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
            for (auto *action : manage->menu()->actions())
                if (action->text() == QStringLiteral("设置")) {
                    action->trigger();
                    break;
                }
            QCOMPARE(window.savedState().value("preferences").toObject().value("font_size").toInt(),
                     13);
            QCOMPARE(
                window.savedState().value("preferences").toObject().value("batch_limit").toInt(),
                2);
            const int id = window.open("json");
            auto *session = window.sessionView(id)->findChild<QPushButton *>("sessionMenu");
            QVERIFY(session);
            QTimer::singleShot(0, &window, [] {
                if (auto *dialog =
                        qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                    dialog->setTextValue(QStringLiteral("工作副本 A"));
                    dialog->accept();
                }
            });
            for (auto *action : session->menu()->actions())
                if (action->text() == QStringLiteral("重命名会话")) {
                    action->trigger();
                    break;
                }
            QVERIFY(window.sessionView(id)->findChild<QLabel *>("heading")->text().contains(
                QStringLiteral("工作副本 A")));
            window.addSession();
            window.findChild<QSplitter *>("navigationSplitter")->setSizes({240, 960});
            QVERIFY(window.saveState());
        }
        {
            Workbench window(readCatalog(packages).tools, config);
            window.show();
            QTest::qWait(20);
            const auto state = window.savedState();
            QCOMPARE(window.sessionCount(), 2);
            QCOMPARE(state.value("preferences").toObject().value("font_size").toInt(), 13);
            bool title = false, draft = false;
            for (const auto &value : state.value("sessions").toArray()) {
                title |=
                    value.toObject().value("title") == QJsonValue(QStringLiteral("工作副本 A"));
                draft |= value.toObject().value("draft").toBool();
            }
            QVERIFY(title);
            QVERIFY(draft);
            QVERIFY(qAbs(state.value("navigation_width").toInt() - 240) < 5);
        }
    }
    void closingBusySessionWaitsForItsJob() {
        QTemporaryDir root;
        const auto packages = root.path() + "/tools";
        PackageManager manager(packages);
        QCOMPARE(manager.install(package("json")), QString());
        Workbench window(readCatalog(packages).tools,
                         {root.path() + "/data", packages,
                          QCoreApplication::applicationDirPath() + "/ModynToolbox.WorkerHost.exe"});
        window.show();
        const int id = window.open("json");
        window.editor(id)->setPlainText("{\"x\":1}");
        window.panel(id)->trigger("format");
        QTimer::singleShot(0, &window, [] {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
                for (auto *button : box->buttons())
                    if (button->text() == QStringLiteral("完成后关闭")) {
                        button->click();
                        break;
                    }
        });
        window.closeSession(id);
        QTRY_COMPARE_WITH_TIMEOUT(window.sessionCount(), 0, 10000);
        QVERIFY(!window.jobScheduler()->busy());
    }
    void missingDraftFilePreservesItsReference() {
        QTemporaryDir root;
        const auto packages = root.path() + "/tools";
        PackageManager manager(packages);
        QCOMPARE(manager.install(package("rename")), QString());
        const auto input = root.path() + "/source.txt";
        write(input, "sample");
        const WorkbenchConfig config{root.path() + "/data", packages,
                                     QCoreApplication::applicationDirPath() +
                                         "/ModynToolbox.WorkerHost.exe"};
        {
            Workbench window(readCatalog(packages).tools, config);
            const int id = window.open("rename");
            window.panel(id)->restore({{"files", QJsonArray{input}}});
            QTRY_VERIFY(window.panel(id)
                            ->findChild<QTableWidget *>("inputFiles")
                            ->item(0, 1)
                            ->text()
                            .endsWith("KB"));
            window.setDraft(id, true);
            QVERIFY(window.saveState());
        }
        QFile::remove(input);
        {
            Workbench window(readCatalog(packages).tools, config);
            const int id = window.open("rename");
            QTRY_VERIFY(window.panel(id)
                            ->findChild<QLabel *>("inputFileNotice")
                            ->text()
                            .contains(QStringLiteral("缺失")));
            QCOMPARE(window.panel(id)->input().value("files").toArray().first().toString(), input);
        }
    }
    void invalidDraftSurvivesAutomaticSave() {
        QTemporaryDir root;
        const auto packages = root.path() + "/tools";
        PackageManager manager(packages);
        QCOMPARE(manager.install(package("json")), QString());
        const auto tool = readCatalog(packages).tools.first();
        const QString oversized(5 * 1024 * 1024 + 1, 'x');
        const auto data = root.path() + "/data";
        {
            StateRepository repository(data);
            QVERIFY(repository.open());
            const QJsonObject snapshot{{"text", oversized},
                                       {"result", QJsonObject{{"text", "stale"}}}};
            const QJsonObject session{{"tool", "json"},
                                      {"version", tool.version},
                                      {"uuid", "draft"},
                                      {"draft", true},
                                      {"snapshot", snapshot}};
            QVERIFY(repository.save({{"sessions", QJsonArray{session}}, {"active", "draft"}}));
        }
        {
            Workbench window(readCatalog(packages).tools, {data, packages,
                                                           QCoreApplication::applicationDirPath() +
                                                               "/ModynToolbox.WorkerHost.exe"});
            const auto saved = window.savedState()
                                   .value("sessions")
                                   .toArray()
                                   .first()
                                   .toObject()
                                   .value("snapshot")
                                   .toObject();
            QCOMPARE(saved.value("text").toString(), oversized);
            QVERIFY(!saved.contains("result"));
            QVERIFY(window.saveState());
        }
        StateRepository repository(data);
        QVERIFY(repository.open());
        QCOMPARE(repository.state()
                     .value("sessions")
                     .toArray()
                     .first()
                     .toObject()
                     .value("snapshot")
                     .toObject()
                     .value("text")
                     .toString(),
                 oversized);
    }
    void managedExportPreservesResultAndSupportsCancellation() {
        QTemporaryDir root;
        const auto packages = root.path() + "/tools";
        PackageManager manager(packages);
        QCOMPARE(manager.install(package("json")), QString());
        Workbench window(readCatalog(packages).tools,
                         {root.path() + "/data", packages,
                          QCoreApplication::applicationDirPath() + "/ModynToolbox.WorkerHost.exe"});
        const int id = window.open("json");
        window.editor(id)->setPlainText("{\"x\":1}");
        QSignalSpy finished(window.jobScheduler(), &Scheduler::finished);
        window.panel(id)->trigger("format");
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 10000);
        const auto original = window.panel(id)->resultText();
        const auto path = root.path() + "/result.txt";
        emit window.panel(id)->exportRequested(path, {{"text", original}}, true);
        window.detachSession(id);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 10000);
        QCOMPARE(finished.last().at(4).toString(), QString("succeeded"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original.toUtf8());
        file.close();
        QCOMPARE(window.panel(id)->resultText(), original);
        const auto tool = readCatalog(packages).tools.first();
        const auto job = window.jobScheduler()->submitExport(tool, "cancelled-export", 1, path,
                                                             {{"text", "replacement"}}, true);
        QVERIFY(!job.isEmpty());
        window.jobScheduler()->cancel(job);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 3, 10000);
        QCOMPARE(finished.last().at(4).toString(), QString("cancelled"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original.toUtf8());
    }
    void actualImageBackendAndOriginalPreserved() {
        QTemporaryDir root;
        const auto source = root.path() + "/source.png";
        QImage image(40, 20, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QVERIFY(image.save(source));
        QString error;
        const auto path = package("image");
        const auto tool =
            definitionFromManifest(path, Files::readObject(path + "/manifest.json", 65536), &error);
        QVERIFY(error.isEmpty());
        const auto digest = Files::sha256(source);
        Scheduler scheduler(root.path() + "/data", QCoreApplication::applicationDirPath() +
                                                       "/ModynToolbox.WorkerHost.exe");
        QSignalSpy finished(&scheduler, &Scheduler::finished);
        QVERIFY(!scheduler
                     .submit(tool, "image-session", 1, "compress",
                             {{"files", QJsonArray{source}},
                              {"params", QJsonObject{{"format", "JPEG"}, {"quality", 80}}}})
                     .isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 30000);
        const auto result = finished.first().at(3).toJsonObject();
        QCOMPARE(finished.first().at(4).toString(), QString("succeeded"));
        const auto output = result.value("artifacts").toArray().first().toString();
        QVERIFY(QFile::exists(output));
        QCOMPARE(Files::sha256(source), digest);
        QImage saved(output);
        QCOMPARE(saved.size(), QSize(40, 20));
        QVERIFY(saved.pixelColor(0, 0).green() > 240);
    }
};
QTEST_MAIN(SystemTests)
#include "test_system.moc"

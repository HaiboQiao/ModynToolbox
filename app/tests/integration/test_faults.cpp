#include "catalog/package_manager.h"
#include "core/files.h"
#include "jobs/scheduler.h"
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#define NOMINMAX
#include <windows.h>
class FaultTests : public QObject {
    Q_OBJECT
    QTemporaryDir folder;
    ToolDefinition tool;
    QString bridge = QCoreApplication::applicationDirPath() + "/ModynToolbox.WorkerHost.exe";
  private slots:
    void initTestCase() {
        const auto source = qEnvironmentVariable("MODYN_SOURCE_ROOT");
        const auto package = Files::readObject(source + "/.local/tools/json/latest.json", 4096)
                                 .value("package")
                                 .toString();
        const auto destination = folder.path() + "/package";
        QString error;
        QVERIFY2(Files::copyTree(package, destination, &error), qPrintable(error));
        QFile::remove(destination + "/backend/adapter.py");
        QVERIFY(QFile::copy(source + "/app/tests/fixtures/backend.py",
                            destination + "/backend/adapter.py"));
        auto manifest = Files::readObject(destination + "/manifest.json", 65536);
        manifest["tool_id"] = "fixture";
        manifest["actions"] =
            QJsonArray{QJsonObject{{"id", "run"}, {"label", "run"}, {"timeout_seconds", 2}}};
        auto limits = manifest.value("limits").toObject();
        limits["memory_mb"] = 128;
        manifest["limits"] = limits;
        QJsonObject hashes;
        QDirIterator iterator(destination, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const auto file = iterator.next();
            const auto relative = QDir(destination).relativeFilePath(file);
            if (relative != "checksums.json" && relative != "manifest.json")
                hashes[relative] = Files::sha256(file);
        }
        QVERIFY(Files::writeObject(destination + "/checksums.json", hashes));
        manifest["integrity_sha256"] = Files::sha256(destination + "/checksums.json");
        QVERIFY(Files::writeObject(destination + "/manifest.json", manifest));
        tool = definitionFromManifest(destination, manifest, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }
    void malformedAndCrashedWorkers_data() {
        QTest::addColumn<QString>("mode");
        for (const auto &mode : {"crash", "flood", "wrong-id", "memory"})
            QTest::newRow(mode) << QString(mode);
    }
    void malformedAndCrashedWorkers() {
        QFETCH(QString, mode);
        QTemporaryDir root;
        Scheduler scheduler(root.path(), bridge);
        QSignalSpy finished(&scheduler, &Scheduler::finished);
        int ticks = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++ticks; });
        heartbeat.start(10);
        QVERIFY(
            !scheduler
                 .submit(tool, "fault-session", 1, "run", {{"params", QJsonObject{{"mode", mode}}}})
                 .isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 15000);
        QCOMPARE(finished.first().at(4).toString(), QString("failed"));
        QVERIFY(ticks > 5);
        QVERIFY(!scheduler.busy());
    }
    void cooperativeCancelAndCriticalWait() {
        QTemporaryDir root;
        Scheduler scheduler(root.path(), bridge);
        QSignalSpy finished(&scheduler, &Scheduler::finished);
        const auto id = scheduler.submit(tool, "cancel-session", 1, "run",
                                         {{"params", QJsonObject{{"mode", "cancel"}}}});
        QTRY_COMPARE_WITH_TIMEOUT(scheduler.record(id).state, QString("running"), 5000);
        QTest::qWait(150);
        scheduler.cancel(id);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 10000);
        QCOMPARE(finished.first().at(4).toString(), QString("cancelled"));
        const auto critical = scheduler.submit(tool, "critical-session", 1, "run",
                                               {{"params", QJsonObject{{"mode", "critical"}}}});
        QTRY_VERIFY_WITH_TIMEOUT(scheduler.critical(), 5000);
        scheduler.cancel(critical);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 10000);
        QCOMPARE(finished.last().at(4).toString(), QString("succeeded"));
    }
    void descendantsTerminateWithJob() {
        QTemporaryDir root;
        Scheduler scheduler(root.path(), bridge);
        QSignalSpy finished(&scheduler, &Scheduler::finished);
        scheduler.submit(tool, "tree-session", 1, "run",
                         {{"params", QJsonObject{{"mode", "tree"}}}});
        const auto pidPath = root.path() + "/tools/fixture/child.pid";
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(pidPath), 5000);
        QFile pidFile(pidPath);
        QVERIFY(pidFile.open(QIODevice::ReadOnly));
        const auto pid = pidFile.readAll().toULong();
        const auto process =
            OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        QVERIFY(process != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 10000);
        QCOMPARE(WaitForSingleObject(process, 3000), DWORD(WAIT_OBJECT_0));
        CloseHandle(process);
    }
};
QTEST_GUILESS_MAIN(FaultTests)
#include "test_faults.moc"

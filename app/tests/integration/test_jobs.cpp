#include "jobs/scheduler.h"
#include "core/files.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
class JobSystemTests : public QObject {
    Q_OBJECT
    ToolDefinition tool(const QString &id) {
        const auto root = qEnvironmentVariable("MODYN_SOURCE_ROOT");
        const auto record = Files::readObject(root + "/.local/tools/" + id + "/latest.json", 4096);
        const auto path = record.value("package").toString();
        QString error;
        auto definition =
            definitionFromManifest(path, Files::readObject(path + "/manifest.json", 65536), &error);
        if (!error.isEmpty())
            qWarning() << error;
        return definition;
    }
  private slots:
    void realJsonWorker() {
        QTemporaryDir root;
        Scheduler scheduler(root.path(), QCoreApplication::applicationDirPath() +
                                             "/ModynToolbox.WorkerHost.exe");
        QSignalSpy finished(&scheduler, &Scheduler::finished);
        const auto id = scheduler.submit(
            tool("json"), "json-session", 1, "format",
            {{"text", "{\"n\":123456789012345678901234567890,\"x\":1.234567890123456789}"},
             {"params", QJsonObject{}},
             {"files", QJsonArray{}}});
        QVERIFY(!id.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 30000);
        const auto result = finished.at(0).at(3).toJsonObject();
        QVERIFY2(finished.at(0).at(4).toString() == "succeeded",
                 qPrintable(QString::fromUtf8(QJsonDocument(result).toJson())));
        QVERIFY(result.value("text").toString().contains("123456789012345678901234567890"));
        QVERIFY(result.value("text").toString().contains("1.234567890123456789"));
    }
    void realRenamePreviewApplyUndo() {
        QTemporaryDir root;
        const auto path = root.path() + "/sample.txt";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("original");
        file.close();
        Scheduler scheduler(root.path() + "/data", QCoreApplication::applicationDirPath() +
                                                       "/ModynToolbox.WorkerHost.exe");
        QSignalSpy finished(&scheduler, &Scheduler::finished);
        auto definition = tool("rename");
        auto id = scheduler.submit(
            definition, "rename-session", 1, "preview",
            {{"files", QJsonArray{path}}, {"params", QJsonObject{{"prefix", "new_"}}}});
        QVERIFY(!id.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 30000);
        auto result = finished.at(0).at(3).toJsonObject();
        QVERIFY2(finished.at(0).at(4).toString() == "succeeded",
                 qPrintable(result.value("message").toString()));
        id = scheduler.submit(definition, "rename-session", 1, "apply",
                              {{"files", QJsonArray{path}},
                               {"params", QJsonObject{}},
                               {"context", result.value("context")}});
        QVERIFY(!id.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 30000);
        result = finished.at(1).at(3).toJsonObject();
        QVERIFY2(finished.at(1).at(4).toString() == "succeeded",
                 qPrintable(result.value("message").toString()));
        QVERIFY(QFile::exists(root.path() + "/new_sample.txt"));
        id = scheduler.submit(definition, "rename-session", 2, "undo",
                              {{"files", QJsonArray{}}, {"params", QJsonObject{}}});
        QVERIFY(!id.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 3, 30000);
        result = finished.at(2).at(3).toJsonObject();
        QVERIFY2(finished.at(2).at(4).toString() == "succeeded",
                 qPrintable(result.value("message").toString()));
        QVERIFY(QFile::exists(path));
    }
};
QTEST_GUILESS_MAIN(JobSystemTests)
#include "test_jobs.moc"

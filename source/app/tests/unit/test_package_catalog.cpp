#include "catalog/package_catalog.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
class CatalogTests : public QObject {
    Q_OBJECT
    void writePackage(const QString &root, const QString &id, const QString &version, int schema=1) {
        const auto path = root + "/" + id + "/" + version;
        QVERIFY(QDir().mkpath(path));
        QFile file(path + "/manifest.json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        QJsonObject manifest{{"manifest_version",schema},{"tool_id",id},{"package_version",version},{"name",id}};
        file.write(QJsonDocument(manifest).toJson());
    }
private slots:
    void discoverAndDelete() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        writePackage(root.path(), "alpha", "1.0.0");
        writePackage(root.path(), "beta", "1.0.0");
        QCOMPARE(readCatalog(root.path()).tools.size(), 2);
        // Only test-owned temporary files are removed.
        QVERIFY(QDir(root.path() + "/alpha").removeRecursively());
        const auto after = readCatalog(root.path());
        QCOMPARE(after.tools.size(), 1);
        QCOMPARE(after.tools.front().id, QString("beta"));
        writePackage(root.path(), "fourth", "1.0.0");
        QCOMPARE(readCatalog(root.path()).tools.size(), 2);
    }
    void hundredMetadataPackages() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        for (int index = 0; index < 100; ++index) {
            writePackage(root.path(), QString("tool-%1").arg(index), "1.0.0");
        }
        const auto result = readCatalog(root.path());
        QCOMPARE(result.tools.size(), 100);
        QVERIFY(result.errors.isEmpty());
    }
    void numericVersions() {
        QTemporaryDir root;
        writePackage(root.path(), "alpha", "1.2.0");
        writePackage(root.path(), "alpha", "1.10.0");
        QCOMPARE(readCatalog(root.path()).tools.front().version, QString("1.10.0"));
    }
    void corruptAndOversized() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        writePackage(root.path(), "alpha", "1.0.0");
        QFile manifest(root.path() + "/alpha/1.0.0/manifest.json");
        QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
        manifest.write("{broken");
        manifest.close();
        QCOMPARE(readCatalog(root.path()).errors.size(), 1);
        QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
        manifest.write(QByteArray(65537, ' '));
        manifest.close();
        QVERIFY(readCatalog(root.path()).tools.isEmpty());
        QCOMPARE(readCatalog(root.path()).errors.size(), 1);
        QVERIFY(readCatalog(QString()).tools.isEmpty());
    }
    void minimumHostUsesProductVersion() {
        QTemporaryDir root;
        QJsonObject manifest{{"manifest_version",1},{"tool_id","compatible"},{"package_version","1.0.0"},
                             {"name","Compatible"},{"min_host_version",QStringLiteral(MODYN_VERSION)}};
        QString error;
        definitionFromManifest(root.path(),manifest,&error);
        QVERIFY(error.isEmpty());
        manifest["min_host_version"]="99.0.0";
        definitionFromManifest(root.path(),manifest,&error);
        QVERIFY(!error.isEmpty());
    }
    void completeFrontendContract() {
        QTemporaryDir root;QVERIFY(root.isValid());QVERIFY(QDir().mkpath(root.path()+"/frontend"));
        QFile page(root.path()+"/frontend/index.html");QVERIFY(page.open(QIODevice::WriteOnly));page.write("<html></html>");page.close();
        QJsonObject front{{"type","web"},{"entry","frontend/index.html"},{"bridge_version",1},{"state_version",1},{"capabilities",QJsonArray{"state"}}};
        QJsonObject manifest{{"manifest_version",2},{"tool_id","pure-page"},{"package_version","1.0.0"},{"name","Pure"},{"frontend",front}};
        auto read=[&](const QJsonObject &f){manifest["frontend"]=f;QString error;auto tool=definitionFromManifest(root.path(),manifest,&error);return qMakePair(tool,error);};
        const auto valid=read(front);QVERIFY(valid.second.isEmpty());QVERIFY(valid.first.entryProgram.isEmpty());QCOMPARE(valid.first.frontendCapabilities,QStringList{"state"});
        for(const QJsonValue &caps:QJsonArray{QJsonArray{"jobs","jobs"},QJsonArray{"unknown"},QJsonValue("jobs")}){auto bad=front;bad["capabilities"]=caps;QVERIFY(!read(bad).second.isEmpty());}
        for(const auto &path:QStringList{"../index.html","frontend/../manifest.json","frontend/missing.html","frontend/"}){auto bad=front;bad["entry"]=path;QVERIFY(!read(bad).second.isEmpty());}
        auto bad=front;bad["bridge_version"]=99;QVERIFY(!read(bad).second.isEmpty());bad=front;bad["state_version"]=1.5;QVERIFY(!read(bad).second.isEmpty());
        manifest.remove("frontend");QString error;definitionFromManifest(root.path(),manifest,&error);QVERIFY(!error.isEmpty());
    }
    void invalidMetadata() {
        QTemporaryDir root;
        writePackage(root.path(), "bad", "1.0.0", 99);
        const auto result = readCatalog(root.path());
        QVERIFY(result.tools.isEmpty());
        QCOMPARE(result.errors.size(), 1);
    }
};
QTEST_APPLESS_MAIN(CatalogTests)
#include "test_package_catalog.moc"

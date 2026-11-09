#include "catalog/package_catalog.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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

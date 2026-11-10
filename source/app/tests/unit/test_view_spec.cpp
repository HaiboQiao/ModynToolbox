#include "ui/view_spec.h"
#include "ui/components/tool_view.h"
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>
class ViewSpecTests : public QObject {
    Q_OBJECT
    void write(const QString &root,const QJsonObject &object) {
        QFile file(root+"/ui.json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(object).toJson());
    }
private slots:
    void initTestCase() {
        const auto fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
        QVERIFY(QFontDatabase::addApplicationFont(fonts + "segoeui.ttf") >= 0);
        QVERIFY(QFontDatabase::addApplicationFont(fonts + "msyh.ttc") >= 0);
    }
    void legacyFallback() {
        QTemporaryDir root;
        const auto spec=loadViewSpec(root.path());
        QVERIFY(spec.error.isEmpty());
        QCOMPARE(spec.kind,QString("text"));
        const auto rendered=renderToolView(spec);
        std::unique_ptr<QWidget> owner(rendered.widget);
        QVERIFY(rendered.editor);
    }
    void requiredViewMissingIsLocalError() {
        QTemporaryDir root;
        QVERIFY(loadViewSpec(root.path()).error.isEmpty());
        QVERIFY(!loadViewSpec(root.path(),true).error.isEmpty());
        QVERIFY(!loadViewSpec(QString(),true).error.isEmpty());
    }
    void validFields() {
        QTemporaryDir root;
        write(root.path(),{{"view_version",1},{"kind","files"},{"extensions",QJsonArray{"png"}},{"fields",QJsonArray{
            QJsonObject{{"id","format"},{"label","Format"},{"type","choice"},{"options",QJsonArray{"PNG","WebP"}},{"default","WebP"}},
            QJsonObject{{"id","quality"},{"label","Quality"},{"type","range"},{"minimum",1},{"maximum",100},{"default",80}}
        }}});
        const auto spec=loadViewSpec(root.path());
        QVERIFY2(spec.error.isEmpty(),qPrintable(spec.error));
        QCOMPARE(spec.fields.size(),2);
        QCOMPARE(spec.fields.last().number,80);
        const auto rendered=renderToolView(spec);
        std::unique_ptr<QWidget> owner(rendered.widget);
        QVERIFY(!rendered.editor);
    }
    void rejectsInvalidVersionsAndControls() {
        QTemporaryDir root;
        write(root.path(),{{"view_version",99},{"kind","files"}});
        QVERIFY(!loadViewSpec(root.path()).error.isEmpty());
        write(root.path(),{{"view_version",1},{"kind","script"}});
        QVERIFY(!loadViewSpec(root.path()).error.isEmpty());
        write(root.path(),{{"view_version",1},{"kind","files"},{"fields",QJsonArray{
            QJsonObject{{"id","x"},{"label","X"},{"type","choice"},{"options",QJsonArray{"a"}},{"default","missing"}}
        }}});
        const auto spec=loadViewSpec(root.path());
        QVERIFY(!spec.error.isEmpty());
        const auto rendered=renderToolView(spec);
        std::unique_ptr<QWidget> owner(rendered.widget);
        QVERIFY(rendered.widget->findChild<QLabel *>("viewError"));
    }
    void limitsAndDuplicateFields() {
        QTemporaryDir root;
        const QJsonObject field{{"id","a"},{"label","A"},{"type","text"},{"default",""}};
        write(root.path(),{{"view_version",1},{"kind","text"},{"fields",QJsonArray{field,field}}});
        QVERIFY(!loadViewSpec(root.path()).error.isEmpty());
        QJsonArray fields;
        for (int i=0;i<13;++i) fields.append(field);
        write(root.path(),{{"view_version",1},{"kind","text"},{"fields",fields}});
        QVERIFY(!loadViewSpec(root.path()).error.isEmpty());
        QFile file(root.path()+"/ui.json");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QByteArray(65537,' '));
        file.close();
        QVERIFY(!loadViewSpec(root.path()).error.isEmpty());
    }
    void invalidRangeAndExtensions() {
        QTemporaryDir root;
        write(root.path(),{{"view_version",1},{"kind","files"},{"extensions",QJsonArray{"../png"}}});
        QVERIFY(!loadViewSpec(root.path()).error.isEmpty());
        write(root.path(),{{"view_version",1},{"kind","files"},{"fields",QJsonArray{
            QJsonObject{{"id","quality"},{"label","Quality"},{"type","range"},{"minimum",80},{"maximum",20},{"default",50}}
        }}});
        QVERIFY(!loadViewSpec(root.path()).error.isEmpty());
    }
};
QTEST_MAIN(ViewSpecTests)
#include "test_view_spec.moc"

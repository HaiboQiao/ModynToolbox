#include "ui/view_spec.h"
#include "ui/components/tool_panel.h"
#include "ui/theme/theme.h"
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QDir>
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
        Theme::apply();
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
    void legacyDuplicateChoiceCompatibility() {
        QTemporaryDir root;
        QJsonObject ui{{"view_version",1},{"kind","text"},{"fields",QJsonArray{QJsonObject{{"id","choice"},{"label","Choice"},{"type","choice"},{"options",QJsonArray{"x","x"}},{"default","x"}}}}};
        write(root.path(),ui); QVERIFY(loadViewSpec(root.path()).error.isEmpty());
        ui["view_version"]=3;write(root.path(),ui);QVERIFY(!loadViewSpec(root.path()).error.isEmpty());
    }
    void dynamicFieldsAndStableValues() {
        QTemporaryDir root;
        QJsonArray fields{
            QJsonObject{{"id","sheet"},{"label","工作表"},{"type","choice"},{"default",""},{"compact",true},{"searchable",true},{"options_source","analysis.sheets"}},
            QJsonObject{{"id","column"},{"label","列"},{"type","choice"},{"default",""},{"compact",true},{"options_source","analysis.columns"},{"options_depends_on","sheet"}},
            QJsonObject{{"id","mode"},{"label","方式"},{"type","choice"},{"default","auto"},{"advanced",true},{"options",QJsonArray{QJsonObject{{"label","自动"},{"value","auto"}},QJsonObject{{"label","手动"},{"value","manual"}}}}},
            QJsonObject{{"id","override"},{"label","覆盖"},{"type","text"},{"default",""},{"advanced",true},{"help","自动失败才填"},{"placeholder","空=自动"}}
        };
        write(root.path(),{{"view_version",3},{"kind","text"},{"fields",fields}});
        const auto view=loadViewSpec(root.path()); QVERIFY2(view.error.isEmpty(),qPrintable(view.error));
        ToolDefinition tool; ToolPanel panel(tool,view); panel.resize(900,800); panel.show();
        auto *sheet=panel.findChild<QComboBox *>("sheet"); auto *column=panel.findChild<QComboBox *>("column");
        QVERIFY(sheet && column); QCOMPARE(sheet->count(),0);
        QJsonObject analysis{{"sheets",QJsonArray{QJsonObject{{"label","表一"},{"value","s1"}},QJsonObject{{"label","表二"},{"value","s2"}}}},
            {"columns",QJsonObject{{"s1",QJsonArray{"VIN","时间"}},{"s2",QJsonArray{"序号"}}}}};
        panel.setResult({{"context",QJsonObject{{"analysis",analysis}}}});
        sheet->setCurrentIndex(sheet->findData("s1")); QCOMPARE(column->count(),2);
        column->setCurrentIndex(column->findData("VIN"));
        QCOMPARE(panel.input().value("params").toObject().value("sheet").toString(),QString("s1"));
        QVERIFY(panel.input().value("context").toObject().contains("analysis"));
        auto saved=panel.snapshot(); ToolPanel restored(tool,view); QVERIFY(restored.restore(saved));
        QCOMPARE(restored.input().value("params").toObject().value("column").toString(),QString("VIN"));
        sheet->setCurrentIndex(sheet->findData("s2")); QCOMPARE(column->count(),1); QCOMPARE(column->currentIndex(),-1);
        auto *toggle=panel.findChild<QPushButton *>("advancedToggle"); auto *advanced=panel.findChild<QWidget *>("advancedFields");
        QVERIFY(advanced->isHidden()); toggle->click(); QVERIFY(!advanced->isHidden());
        QCOMPARE(panel.findChild<QLineEdit *>("override")->placeholderText(),QStringLiteral("空=自动"));
        const auto folder=qEnvironmentVariable("MODYN_SOURCE_ROOT")+"/.local/logs";
        QVERIFY(!qEnvironmentVariable("MODYN_SOURCE_ROOT").isEmpty()); QDir().mkpath(folder);
        for (int font : {12,14,18}) {
            Theme::setPreferences(font,font==12);
            panel.resize(900,800); QTest::qWait(30);
            QVERIFY(panel.grab().save(folder+QString("/EXT-1-fields-wide-%1.png").arg(font)));
            panel.resize(380,800); QTest::qWait(30);
            QCOMPARE(panel.input().value("params").toObject().value("sheet").toString(),QString("s2"));
            QVERIFY(panel.grab().save(folder+QString("/EXT-1-fields-narrow-%1.png").arg(font)));
        }
        Theme::setPreferences(14,false);
        panel.editor()->setPlainText("another input"); QCOMPARE(sheet->count(),0);
        fields[0]=QJsonObject{{"id","bad"},{"label","Bad"},{"type","choice"},{"default",""},{"options_source","../escape"}};
        write(root.path(),{{"view_version",3},{"kind","text"},{"fields",fields}});
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

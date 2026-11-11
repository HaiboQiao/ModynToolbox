#include "core/files.h"
#include "ui/components/tool_panel.h"
#include "ui/components/video_editor.h"
#include "ui/components/video_surface.h"
#include "ui/theme/theme.h"
#include "ui/view_spec.h"
#include <QApplication>
#include <QAudioOutput>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QPushButton>
#include <QTemporaryDir>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtTest>
class VideoTests : public QObject {
    Q_OBJECT
    QString root, sample;
    QJsonObject media;
    ViewSpec view;
    ToolDefinition definition;
  private slots:
    void initTestCase() {
        root = qEnvironmentVariable("MODYN_SOURCE_ROOT");
        const auto fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
        QFontDatabase::addApplicationFont(fonts + "segoeui.ttf");
        QFontDatabase::addApplicationFont(fonts + "msyh.ttc");
        Theme::apply();
        const auto latest = Files::readObject(root + "/.local/video-tests/latest.json", 4096);
        sample = latest.value("sample").toString();
        QVERIFY(QFileInfo::exists(sample));
        const QFileInfo info(sample);
        media = {{"source", sample},
                 {"identity", "fixture"},
                 {"duration_ms", 6000},
                 {"width", 640},
                 {"height", 360},
                 {"size_bytes", double(info.size())},
                 {"modified_ms", double(info.lastModified().toMSecsSinceEpoch())},
                 {"codec", "h264"},
                 {"audio_tracks", QJsonArray{QJsonObject{{"index", 1}, {"codec", "aac"}}}}};
        view = loadViewSpec(root + "/tools/video-clip/integration", true);
        QVERIFY2(view.error.isEmpty(), qPrintable(view.error));
        definition.id = "fixture-video";
        definition.executable = true;
        definition.uiSchemaVersion = 2;
        definition.limits = {{"max_files", 1}, {"max_input_bytes", 5242880}};
        for (const auto &id : {"export", "prepare", "probe"}) {
            ActionSpec a;
            a.id = id;
            a.label = QString(id) == "export"    ? QStringLiteral("导出视频")
                      : QString(id) == "prepare" ? QStringLiteral("预览快速截取区间")
                                                 : QStringLiteral("重新分析");
            definition.actions.append(a);
        }
    }
    void schemaVersionAndBindingValidation() {
        QCOMPARE(view.version, 2);
        QCOMPARE(view.mediaProbe, QString("probe"));
        QTemporaryDir dir;
        auto raw = Files::readObject(root + "/tools/video-clip/integration/ui.json", 65536);
        raw["view_version"] = 1;
        QVERIFY(Files::writeObject(dir.path() + "/ui.json", raw));
        QVERIFY(!loadViewSpec(dir.path(), true).error.isEmpty());
        raw["view_version"] = 2;
        raw["media"] = QJsonObject{{"probe_action", "missing"}};
        QVERIFY(Files::writeObject(dir.path() + "/ui.json", raw));
        QVERIFY(!loadViewSpec(dir.path(), true).error.isEmpty());
    }
    void sourceMetadataAndRange() {
        VideoEditor editor({"mp4"});
        editor.setSource(sample);
        QVERIFY(editor.setMedia(media));
        editor.restoreSelection({{"start_ms", 1240}, {"end_ms", 4240}, {"audio_index", 1}});
        QCOMPARE(editor.selection().value("start_ms").toDouble(), 1240.0);
        auto *start = editor.findChild<QLineEdit *>("videoStart");
        auto *end = editor.findChild<QLineEdit *>("videoEnd");
        start->setText("00:00:02.000");
        QMetaObject::invokeMethod(start, "editingFinished");
        QCOMPARE(editor.selection().value("start_ms").toDouble(), 2000.0);
        end->setText("00:00:01.000");
        QMetaObject::invokeMethod(end, "editingFinished");
        QCOMPARE(editor.selection().value("end_ms").toDouble(), 4240.0);
        auto invalid = media;
        invalid["duration_ms"] = -1;
        QVERIFY(!editor.setMedia(invalid));
        invalid = media;
        invalid["size_bytes"] = 1;
        QVERIFY(!editor.setMedia(invalid));
    }
    void timelineMouseAndKeyboard() {
        RangeTimeline t;
        t.resize(500, 48);
        t.show();
        t.setValues(10000, 2000, 8000, 0);
        QSignalSpy changes(&t, &RangeTimeline::rangeEdited);
        QTest::mousePress(&t, Qt::LeftButton, {}, QPoint(107, 24));
        QTest::mouseMove(&t, QPoint(155, 24));
        QTest::mouseRelease(&t, Qt::LeftButton, {}, QPoint(155, 24));
        QVERIFY(changes.size() > 0);
        auto before = changes.last().first().toLongLong();
        QTest::keyClick(&t, Qt::Key_Right);
        QCOMPARE(changes.last().first().toLongLong(), before + 100);
        QTest::keyClick(&t, Qt::Key_Tab);
        QTest::keyClick(&t, Qt::Key_Left);
        QCOMPARE(changes.last().last().toLongLong(), 7900LL);
        QSignalSpy seeks(&t, &RangeTimeline::seekRequested);
        QTest::mouseClick(&t, Qt::LeftButton, {}, QPoint(250, 24));
        QCOMPARE(seeks.size(), 1);
    }
    void panelStateAndPlanInvalidation() {
        ToolPanel panel(definition, view);
        QSignalSpy requests(&panel, &ToolPanel::actionRequested);
        panel.addPaths({sample});
        QTRY_COMPARE(requests.size(), 1);
        QCOMPARE(requests.first().first().toString(), QString("probe"));
        panel.setResult({{"context", QJsonObject{{"media", media}}}, {"status", "succeeded"}});
        auto *editor = panel.findChild<VideoEditor *>();
        QVERIFY(editor);
        editor->restoreSelection({{"start_ms", 1240}, {"end_ms", 4240}});
        auto input = panel.input();
        QCOMPARE(input.value("files").toArray().size(), 1);
        QCOMPARE(input.value("params").toObject().value("start_ms").toDouble(), 1240.0);
        auto *mode = panel.findChild<QComboBox *>("mode");
        mode->setCurrentText(QStringLiteral("快速截取"));
        auto *exportButton = panel.findChild<QPushButton *>("action_export");
        QVERIFY(!exportButton->isEnabled());
        panel.setResult({{"context", QJsonObject{{"media", media},
                                                 {"plan", QJsonObject{{"fast", true},
                                                                      {"start_ms", 0},
                                                                      {"end_ms", 4240}}}}}});
        QVERIFY(exportButton->isEnabled());
        auto *edit = panel.findChild<QLineEdit *>("videoEnd");
        edit->setText("00:00:04.000");
        QMetaObject::invokeMethod(edit, "editingFinished");
        QVERIFY(!exportButton->isEnabled());
        QVERIFY(panel.input().value("context").toObject().contains("media"));
        auto snapshot = panel.snapshot();
        ToolPanel other(definition, view);
        QVERIFY(other.restore(snapshot));
        QCOMPARE(other.input().value("params").toObject().value("end_ms").toDouble(), 4000.0);
        QVERIFY(!panel.snapshot(false).contains("video"));
    }
    void actualDecodeSeekPauseAndHide() {
        VideoEditor editor({"mp4"});
        editor.resize(680, 680);
        editor.show();
        editor.setSource(sample);
        QVERIFY(editor.setMedia(media));
        auto *player = editor.findChild<QMediaPlayer *>();
        QVERIFY(player);
        editor.findChild<QAudioOutput *>()->setMuted(true);
        auto *surface = editor.findChild<VideoSurface *>();
        QSignalSpy frames(surface->videoSink(), &QVideoSink::videoFrameChanged);
        player->play();
        QTRY_VERIFY_WITH_TIMEOUT(frames.size() >= 3, 10000);
        QVERIFY(player->isPlaying());
        player->pause();
        player->setPosition(2000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(player->position() - 2000) < 200, 5000);
        auto frame = surface->videoSink()->videoFrame();
        QVERIFY(frame.isValid());
        QVERIFY(!frame.toImage().isNull());
        editor.findChild<QCheckBox *>("videoLoop")->setChecked(true);
        player->setPosition(5900);
        player->play();
        QTest::qWait(700);
        QVERIFY(player->isPlaying());
        QVERIFY(player->position() < 2000);
        editor.hide();
        QTRY_VERIFY(!player->isPlaying());
    }
    void layoutMatrixAndScreenshots() {
        for (int font : {12, 14, 18})
            for (bool compact : {false, true}) {
                Theme::setPreferences(font, compact);
                ToolPanel panel(definition, view);
                panel.resize(compact ? 640 : 1100, compact ? 700 : 820);
                panel.show();
                panel.restore(
                    {{"video",
                      QJsonObject{{"source", sample}, {"start_ms", 1000}, {"end_ms", 4000}}},
                     {"context", QJsonObject{{"media", media}}}});
                QTest::qWait(50);
                for (const auto &id : {"videoStart", "videoEnd"}) {
                    auto *e = panel.findChild<QLineEdit *>(id);
                    QVERIFY(e);
                    QVERIFY(e->width() > 100);
                    QVERIFY(e->height() > font + 10);
                    QVERIFY(e->isEnabled());
                }
                const auto output = root + QString("/.local/design/VID-1-%1-%2.png")
                                               .arg(font)
                                               .arg(compact ? "compact" : "standard");
                QVERIFY(panel.grab().save(output));
            }
        Theme::setPreferences(14, false);
    }
};
QTEST_MAIN(VideoTests)
#include "test_video_editor.moc"

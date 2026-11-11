#include <QString>
#include "automation.h"
#include "ui/workbench.h"
#include "ui/components/tool_panel.h"
#include "ui/components/video_editor.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QPushButton>
#include "jobs/scheduler.h"
#include "core/files.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QDir>
class Automation : public QObject {
  public:
    Automation(Workbench *host, QJsonObject script)
        : QObject(host), window(host), script(std::move(script)) {
        clock.start();
        if (window->jobScheduler())
            connect(window->jobScheduler(), &Scheduler::finished, this,
                    [this](const QString &, const QString &id, int, const QJsonObject &result,
                           const QString &) {
                        results[id] = result;
                        if (results.size() > 32)
                            results.remove(results.firstKey());
                    });
    }
    void next() {
        const auto steps = script.value("steps").toArray();
        if (index >= steps.size()) {
            finish({});
            return;
        }
        const auto step = steps[index].toObject();
        const auto op = step.value("op").toString();
        QString error;
        if (op == "wait") {
            if (window->jobScheduler() && window->jobScheduler()->busy()) {
                if (clock.elapsed() > step.value("timeout_ms").toInt(60000)) {
                    finish("job timeout");
                    return;
                }
                QTimer::singleShot(20, this, [this] { next(); });
                return;
            }
            for (const auto &job : window->jobScheduler()->records()) {
                if (job.created >= since)
                    last = {{"state", job.state},
                            {"result", results.value(job.request.jobId)},
                            {"id", job.request.jobId}};
            }
            if (step.contains("state") && last.value("state") != step.value("state"))
                error = "unexpected job state " + last.value("state").toString();
        } else if (op == "install")
            error = window->installPackage(step.value("path").toString());
        else if (op == "remove")
            error = window->removePackage(step.value("tool").toString(),
                                          step.value("version").toString());
        else if (op == "open") {
            current = window->open(step.value("tool").toString(), step.value("fresh").toBool());
            if (!current)
                error = "tool not found";
        } else if (op == "input") {
            if (auto *body = window->panel(current))
                body->restore(step.value("value").toObject());
            else
                error = "no active panel";
        } else if (op == "run") {
            auto *body = window->panel(current);
            if (!body)
                error = "no active panel";
            else {
                last = {};
                since = QDateTime::currentMSecsSinceEpoch();
                window->runAction(current, step.value("action").toString(),
                                  step.contains("input") ? step.value("input").toObject()
                                                         : body->input(),
                                  step.value("confirmed").toBool());
            }
        } else if (op == "preview") {
            auto *body=window->panel(current);
            auto *editor=body?body->findChild<VideoEditor *>():nullptr;
            if (!editor) error="no media editor";
            else {
                if (auto *audio=editor->findChild<QAudioOutput *>()) audio->setMuted(true);
                if (step.value("play").toBool()) editor->findChild<QPushButton *>("videoPlay")->click();
                else editor->pause();
                if (step.contains("position_ms"))
                    if (auto *player=editor->findChild<QMediaPlayer *>()) player->setPosition(step.value("position_ms").toInteger());
            }
        } else if (op == "cancel") {
            if (auto *body=window->panel(current)) emit body->cancelRequested();
            else error="no active panel";
        } else if (op == "detach")
            window->detachSession(current);
        else if (op == "attach")
            window->attachSession(current);
        else if (op == "pair")
            window->pairSession(step.value("session").toInt(current));
        else if (op == "draft")
            window->setDraft(current, step.value("enabled").toBool());
        else if (op == "close")
            window->closeSession(current, step.value("confirmed").toBool());
        else if (op == "save") {
            if (!window->saveState())
                error = window->storageError();
        } else if (op == "refresh")
            window->refreshTools();
        else if (op == "screenshot") {
            if (!window->grab().save(step.value("path").toString()))
                error = "screenshot failed";
        } else if (op == "mark") {
            if (!Files::writeObject(step.value("path").toString(), {{"ready", true}}))
                error = "marker failed";
        } else if (op == "expect") {
            if (step.contains("sessions") &&
                window->sessionCount() != step.value("sessions").toInt())
                error = "unexpected session count";
            if (step.contains("exists") && !QFileInfo::exists(step.value("exists").toString()))
                error = "expected file missing";
            if (step.contains("detached") &&
                window->isDetached(current) != step.value("detached").toBool())
                error = "unexpected placement";
            if (step.contains("result_contains") &&
                !QString::fromUtf8(
                     QJsonDocument(window->panel(current)->snapshot().value("result").toObject())
                         .toJson())
                     .contains(step.value("result_contains").toString()))
                error = "result content mismatch";
            if (step.contains("text") &&
                (!window->panel(current) ||
                 !window->panel(current)->resultText().contains(step.value("text").toString())))
                error = "result text mismatch";
        } else
            error = "unknown operation " + op;
        QJsonObject record{
            {"step", index}, {"operation", op}, {"elapsed_ms", clock.elapsed()}, {"error", error}};
        if (op == "wait")
            record["job"] = last;
        records.append(record);
        const bool expected = step.value("expect_error").toBool();
        if (expected ? error.isEmpty() : !error.isEmpty()) {
            finish(expected ? "expected rejection was absent" : error);
            return;
        }
        ++index;
        clock.restart();
        QTimer::singleShot(step.value("delay_ms").toInt(20), this, [this] { next(); });
    }

  private:
    void finish(const QString &error) {
        QString writeError;
        Files::writeObject(script.value("report").toString(),
                           {{"success", error.isEmpty()},
                            {"error", error},
                            {"steps", records},
                            {"saved_state", window->savedState()}},
                           &writeError);
        window->saveState();
        QCoreApplication::exit(error.isEmpty() && writeError.isEmpty() ? 0 : 10);
    }
    Workbench *window;
    QMap<QString, QJsonObject> results;
    QJsonObject script, last;
    QJsonArray records;
    int index = 0, current = 0;
    qint64 since = 0;
    QElapsedTimer clock;
};
int startAutomation(Workbench *window, const QString &path) {
    if (QGuiApplication::platformName() != "offscreen")
        return 11;
    QString error;
    const auto script = Files::readObject(path, 8 * 1024 * 1024, &error);
    if (!error.isEmpty() || script.value("report").toString().isEmpty() ||
        script.value("steps").toArray().size() > 1000)
        return 12;
    auto *automation = new Automation(window, script);
    QTimer::singleShot(0, automation, [automation] { automation->next(); });
    return 0;
}

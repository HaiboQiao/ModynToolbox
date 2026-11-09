#include "scheduler.h"
#include "core/files.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QSet>
#include <QUuid>
#include <QtConcurrent>
#include <QJsonDocument>
#include <algorithm>
#define NOMINMAX
#include <windows.h>
namespace {
struct Prepared {
    QStringList keys;
    QString error;
};

bool terminal(const QString &state) {
    return state == "succeeded" || state == "failed" || state == "cancelled" ||
           state == "interrupted";
}
} // namespace
Scheduler::Scheduler(QString root, QString executable, QObject *parent)
    : QObject(parent), dataRoot(std::move(root)), bridge(std::move(executable)) {
    preparation.setMaxThreadCount(1);
    const auto tools =
        QDir(dataRoot + "/tools").entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const auto &tool : tools) {
        const auto jobsRoot = Files::child(dataRoot, "tools/" + tool + "/jobs", true);
        if (jobsRoot.isEmpty())
            continue;
        for (const auto &id :
             QDir(jobsRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
            const auto path = Files::child(jobsRoot, id, true);
            if (path.isEmpty())
                continue;
            const auto marker = Files::readObject(path + "/.modyn-job.json", 4096);
            if (marker.value("owner") != "ModynToolbox" || marker.value("id") != id)
                continue;
            for (const auto &name :
                 {"input.json", "result.json", "resources.json", "preview.png"}) {
                const auto owned = Files::child(path, name, true);
                if (!owned.isEmpty() && QFileInfo(owned).isFile())
                    QFile::remove(owned);
            }
        }
    }
}
Scheduler::~Scheduler() {
    preparation.waitForDone();

    for (auto *worker : running)
        worker->cancel();
    for (auto *worker : running)
        worker->wait();
    for (const auto &job : jobs) {
        if (Files::readObject(job.request.jobRoot + "/.modyn-job.json", 4096)
                .value("id")
                .toString() != job.request.jobId)
            continue;
        for (const auto &name : {"input.json", "result.json", "resources.json", "preview.png"})
            QFile::remove(job.request.jobRoot + "/" + name);
    }
}
QString Scheduler::submit(const ToolDefinition &tool, const QString &session, int revision,
                          const QString &actionId, const QJsonObject &input, JobKind kind) {
    if (activeCount() >= 32 || preparingCount >= 32 || !tool.executable)
        return {};
    const auto action = std::find_if(tool.actions.cbegin(), tool.actions.cend(),
                                     [&](const auto &item) { return item.id == actionId; });
    if (action == tool.actions.cend())
        return {};
    const auto files = input.value("files").toArray();
    if (files.size() > tool.limits.value("max_files").toInt(2000) ||
        input.value("text").toString().toUtf8().size() >
            tool.limits.value("max_input_bytes").toInt(5 * 1024 * 1024))
        return {};
    for (const auto &job : jobs)
        if (job.request.sessionId == session && !terminal(job.state))
            return {};
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto toolRoot = dataRoot + "/tools/" + tool.id;
    const auto jobRoot = toolRoot + "/jobs/" + id;
    JobRecord record;
    record.request = {id, session, jobRoot, toolRoot, bridge, tool, *action, revision};
    record.request.kind = kind;
    record.input = input;
    record.total = files.size();
    if (record.total == 0 && !input.value("text").toString().isEmpty())
        record.total = 1;
    record.cancellable = action->cancellable;
    record.leased = !action->dynamicResources;
    jobs[id] = record;
    emit updated(id);
    ++preparingCount;
    auto *watcher = new QFutureWatcher<Prepared>(this);
    connect(watcher, &QFutureWatcher<Prepared>::finished, this, [this, id, jobRoot, watcher] {
        const auto prepared = watcher->result();
        watcher->deleteLater();
        --preparingCount;
        if (!jobs.contains(id) || jobs[id].state == "cancelled") {
            QFile::remove(jobRoot + "/input.json");
            return;
        }
        jobs[id].input = {};
        if (!prepared.error.isEmpty()) {
            finish(id, {{"message", prepared.error}}, "failed");
            return;
        }
        jobs[id].resources = prepared.keys;
        jobs[id].state = "queued";
        jobs[id].message = QStringLiteral("等待执行");
        queue.append(id);
        emit updated(id);
        dispatch();
    });
    watcher->setFuture(QtConcurrent::run(
        &preparation, [input, jobRoot, id, dynamic = action->dynamicResources, kind] {
            Prepared result;
            if (!Files::writeObject(jobRoot + "/.modyn-job.json",
                                    {{"owner", "ModynToolbox"}, {"id", id}}, &result.error))
                return result;
            const auto bytes = QJsonDocument(input).toJson(QJsonDocument::Compact);
            if (bytes.size() > (kind == JobKind::FileExport ? 40 : 16) * 1024 * 1024) {
                result.error = QStringLiteral("输入快照超限");
                return result;
            }
            if (!Files::writeObject(jobRoot + "/input.json", input, &result.error))
                return result;
            QStringList paths;
            for (const auto &value : input.value("files").toArray())
                paths.append(value.toString());
            if (!dynamic)
                result.keys = Files::resourceKeys(paths);
            return result;
        }));
    return id;
}
int Scheduler::activeCount() const {
    int count = 0;
    for (const auto &job : jobs)
        if (!terminal(job.state))
            ++count;
    return count;
}
bool Scheduler::busy() const {
    return activeCount() > 0;
}
bool Scheduler::critical() const {
    for (const auto &job : jobs)
        if (job.state == "running" && !job.cancellable)
            return true;
    return false;
}
bool Scheduler::compatible(const JobRecord &candidate) const {
    int batches = 0;
    for (auto it = running.cbegin(); it != running.cend(); ++it) {
        if (it.key() == candidate.request.jobId)
            continue;
        const auto &active = jobs[it.key()];
        if (active.request.action.batch)
            ++batches;
        if (active.request.tool.id == candidate.request.tool.id)
            return false;
        if (!active.leased)
            continue;
        if (!active.request.action.writeSources && !candidate.request.action.writeSources)
            continue;
        for (const auto &key : candidate.resources)
            if (active.resources.contains(key))
                return false;
    }
    return !candidate.request.action.batch || batches < batchLimit;
}
void Scheduler::dispatch() {
    while (running.size() < 2) {
        auto candidate = queue.end();
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            if (!compatible(jobs[*it]))
                continue;
            if (candidate == queue.end() ||
                (!jobs[*it].request.action.batch && jobs[*candidate].request.action.batch))
                candidate = it;
        }
        if (candidate == queue.end())
            return;
        const auto id = *candidate;
        queue.erase(candidate);
        auto &job = jobs[id];
        job.state = "running";
        job.message = QStringLiteral("正在执行");
        auto *worker = new JobRunner(job.request, this);
        running[id] = worker;
        connect(worker, &JobRunner::reservation, this,
                [this](const QString &id, const QStringList &keys) {
                    jobs[id].resources = keys;
                    jobs[id].message = QStringLiteral("等待文件资源");
                    reservations.insert(id);
                    emit updated(id);
                    grantReservations();
                });
        connect(worker, &JobRunner::progress, this,
                [this](const QString &id, const QJsonObject &event) {
                    auto &job = jobs[id];
                    job.completed = event.value("completed").toInt();
                    if (event.value("total").toInt() > 0)
                        job.total = event.value("total").toInt();
                    job.message = event.value("message").toString().left(512);
                    job.cancellable =
                        job.request.action.cancellable && event.value("cancellable").toBool(true);
                    emit updated(id);
                });
        connect(worker, &JobRunner::completed, this, &Scheduler::finish);
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        emit updated(id);
        worker->start();
    }
}
void Scheduler::finish(const QString &id, const QJsonObject &result, const QString &outcome) {
    auto &job = jobs[id];
    job.state = outcome;
    job.result = result;
    job.message = result.value("message").toString();
    auto *worker = running.take(id);
    if (worker)
        worker->wait();
    job.input = {};
    const auto artifacts = result.value("artifacts").toArray();
    if (result.contains("total"))
        job.total = result.value("total").toInt(job.total);
    job.completed = result.value("completed")
                        .toInt(qMax(job.completed, int(result.value("rows").toArray().size())));
    if (!result.contains("completed") && outcome == "succeeded")
        job.completed = qMax(job.completed, job.total);
    job.result = {{"status", outcome},
                  {"message", job.message},
                  {"output_count", artifacts.size()},
                  {"failed", result.value("failed").toInt(-1)},
                  {"first_output", artifacts.isEmpty() ? QString() : artifacts.first().toString()},
                  {"partial_effects", result.value("partial_effects").toBool()}};
    for (const auto &name : {"input.json", "result.json", "resources.json"})
        QFile::remove(job.request.jobRoot + "/" + name);
    emit updated(id);
    emit finished(job.request.sessionId, id, job.request.revision, result, outcome);
    if (jobs.size() > 500) {
        QString oldest;
        for (auto it = jobs.cbegin(); it != jobs.cend(); ++it)
            if (terminal(it->state) && it.key() != id &&
                (oldest.isEmpty() || it->created < jobs[oldest].created))
                oldest = it.key();
        if (!oldest.isEmpty())
            jobs.remove(oldest);
    }
    reservations.remove(id);
    grantReservations();
    dispatch();
}
void Scheduler::cancel(const QString &id) {
    if (!jobs.contains(id) || !jobs[id].cancellable)
        return;
    auto &job = jobs[id];
    if (job.state == "preparing" || job.state == "queued") {
        queue.removeAll(id);
        finish(id, {{"status", "cancelled"}, {"message", QStringLiteral("排队任务已取消")}},
               "cancelled");
    } else if (running.contains(id)) {
        job.message = QStringLiteral("正在请求取消，等待当前步骤完成");
        running[id]->cancel();
        emit updated(id);
    }
}

void Scheduler::grantReservations() {
    const auto pending = reservations.values();
    for (const auto &id : pending)
        if (running.contains(id) && compatible(jobs[id])) {
            jobs[id].leased = true;
            reservations.remove(id);
            running[id]->grantResources();
        }
}

void Scheduler::setBatchLimit(int limit) {
    batchLimit = qBound(1, limit, 2);
    dispatch();
}

QString Scheduler::submitExport(const ToolDefinition &tool, const QString &session, int revision,
                                const QString &path, const QJsonObject &document, bool plainText) {
    auto ioTool = tool;
    ioTool.executable = true;
    ioTool.limits["max_files"] = 1;
    ActionSpec action;
    action.id = "export-result";
    action.label = QStringLiteral("导出结果");
    action.writeSources = true;
    action.cancellable = true;
    ioTool.actions = {action};
    return submit(ioTool, session, revision, action.id,
                  {{"files", QJsonArray{path}},
                   {"output_path", path},
                   {"document", document},
                   {"plain_text", plainText}},
                  JobKind::FileExport);
}

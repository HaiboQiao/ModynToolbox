#include "state_repository.h"
#include "jobs/scheduler.h"
#include <QDir>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
StateRepository::StateRepository(const QString &root)
    : dataRoot(root), connection(QUuid::createUuid().toString()) {
}
StateRepository::~StateRepository() {
    close();
}
bool StateRepository::open() {
    if (ready)
        return true;
    close();
    failure.clear();
    QDir().mkpath(dataRoot + "/host");
    database = QSqlDatabase::addDatabase("QSQLITE", connection);
    database.setDatabaseName(dataRoot + "/host/state.sqlite");
    if (!database.open()) {
        failure = database.lastError().text();
        return false;
    }
    QSqlQuery query(database);
    if (!query.exec("PRAGMA user_version") || !query.next()) {
        failure = query.lastError().text();
        return false;
    }
    const int version = query.value(0).toInt();
    if (version > 1) {
        failure = QStringLiteral("数据来自较新版本，请使用兼容程序或恢复备份");
        return false;
    }
    if (version == 0) {
        database.transaction();
        if (!query.exec(
                "CREATE TABLE IF NOT EXISTS state (id INTEGER PRIMARY KEY, body TEXT NOT NULL)") ||
            !query.exec("CREATE TABLE IF NOT EXISTS jobs (id TEXT PRIMARY KEY, created INTEGER NOT "
                        "NULL, body TEXT NOT NULL)") ||
            !query.exec("PRAGMA user_version=1") || !database.commit()) {
            failure = query.lastError().text();
            database.rollback();
            return false;
        }
    }
    if (!query.exec("SELECT id,body FROM state") ||
        !query.exec("SELECT id,created,body FROM jobs")) {
        failure = QStringLiteral("数据库表结构无效，请恢复备份");
        return false;
    }
    if (!query.exec("SELECT body FROM state WHERE id=1")) {
        failure = query.lastError().text();
        return false;
    }
    if (query.next()) {
        const auto bytes = query.value(0).toString().toUtf8();
        QJsonParseError parse;
        const auto doc = QJsonDocument::fromJson(bytes, &parse);
        if (bytes.size() > 64 * 1024 * 1024 || parse.error != QJsonParseError::NoError ||
            !doc.isObject()) {
            failure = QStringLiteral("工作区状态损坏或超限，请恢复备份");
            return false;
        }
        const auto state = doc.object();
        if (state.contains("sessions")) {
            if (!state.value("sessions").isArray() ||
                state.value("sessions").toArray().size() > 100) {
                failure = QStringLiteral("工作区会话结构无效，请恢复备份");
                return false;
            }
            for (const auto &item : state.value("sessions").toArray()) {
                const auto session = item.toObject();
                if (!item.isObject() || !session.value("tool").isString() ||
                    session.value("tool").toString().isEmpty() ||
                    !session.value("version").isString() || !session.value("snapshot").isObject()) {
                    failure = QStringLiteral("工作区会话记录无效，请恢复备份");
                    return false;
                }
            }
        }
    }
    if (!query.exec("PRAGMA journal_mode=WAL") || !query.exec("PRAGMA synchronous=FULL")) {
        failure = query.lastError().text();
        return false;
    }
    ready = true;
    auto restored = history();
    for (const auto &value : restored) {
        auto record = value.toObject();
        if (QStringList{"preparing", "queued", "running"}.contains(
                record.value("state").toString())) {
            record["state"] = "interrupted";
            record["updated"] = QDateTime::currentMSecsSinceEpoch();
            record["message"] =
                QStringLiteral("上次运行中断，未自动重跑；请核对已有输出与恢复记录");
            QSqlQuery update(database);
            update.prepare("UPDATE jobs SET body=? WHERE id=?");
            update.addBindValue(
                QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact)));
            update.addBindValue(record.value("id").toString());
            update.exec();
        }
    }
    return true;
}
void StateRepository::close() {
    if (!database.isValid())
        return;
    if (ready && database.isOpen()) {
        QSqlQuery query(database);
        query.exec("PRAGMA wal_checkpoint(TRUNCATE)");
    }
    ready = false;
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection);
}
QJsonObject StateRepository::state() const {
    if (!ready)
        return {};
    QSqlQuery query(database);
    if (!query.exec("SELECT body FROM state WHERE id=1") || !query.next())
        return {};
    const auto bytes = query.value(0).toString().toUtf8();
    if (bytes.size() > 64 * 1024 * 1024)
        return {};
    return QJsonDocument::fromJson(bytes).object();
}
bool StateRepository::save(const QJsonObject &value) {
    if (!ready)
        return false;
    const auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact);
    if (bytes.size() > 64 * 1024 * 1024) {
        failure = QStringLiteral("保存的草稿总量超过 64 MiB，请减少草稿或先导出");
        return false;
    }
    QSqlQuery query(database);
    query.prepare(
        "INSERT INTO state(id,body) VALUES(1,?) ON CONFLICT(id) DO UPDATE SET body=excluded.body");
    query.addBindValue(QString::fromUtf8(bytes));
    if (!query.exec()) {
        failure = query.lastError().text();
        return false;
    }
    return true;
}
void StateRepository::job(const JobRecord &record, const QString &sessionTitle) {
    if (!ready)
        return;
    QJsonObject value{{"id", record.request.jobId},
                      {"session", record.request.sessionId},
                      {"tool", record.request.tool.id},
                      {"name", record.request.tool.name},
                      {"version", record.request.tool.version},
                      {"action", record.request.action.id},
                      {"revision", record.request.revision},
                      {"state", record.state},
                      {"message", record.message.left(2048)},
                      {"created", record.created},
                      {"completed", record.completed},
                      {"total", record.total}};
    value["session_name"] = sessionTitle;
    value["action_label"] = record.request.action.label;
    value["updated"] = QDateTime::currentMSecsSinceEpoch();
    auto output = record.result.value("first_output").toString();
    const auto absoluteRoot = QDir(dataRoot).absolutePath();
    if (output.startsWith(absoluteRoot + "/", Qt::CaseInsensitive) ||
        QDir::fromNativeSeparators(output).startsWith(absoluteRoot + "/", Qt::CaseInsensitive))
        output = "{data}/" + QDir(dataRoot).relativeFilePath(output);
    value["first_output"] = output;
    value["output_count"] = record.result.value("output_count");
    value["cancellable"] = record.cancellable;
    value["failed"] = record.result.value("failed");
    value["partial_effects"] = record.result.value("partial_effects");
    value["job_dir"] = "tools/" + record.request.tool.id + "/jobs/" + record.request.jobId;
    QSqlQuery query(database);
    query.prepare("INSERT INTO jobs(id,created,body) VALUES(?,?,?) ON CONFLICT(id) DO UPDATE SET "
                  "body=excluded.body");
    query.addBindValue(record.request.jobId);
    query.addBindValue(record.created);
    query.addBindValue(QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)));
    if (!query.exec())
        failure = query.lastError().text();
    query.exec(
        "DELETE FROM jobs WHERE id NOT IN (SELECT id FROM jobs ORDER BY created DESC LIMIT 500)");
}
QJsonArray StateRepository::history() const {
    QJsonArray values;
    if (!ready)
        return values;
    QSqlQuery query(database);
    if (query.exec("SELECT body FROM jobs ORDER BY created DESC LIMIT 500"))
        while (query.next())
            values.append(QJsonDocument::fromJson(query.value(0).toString().toUtf8()).object());
    return values;
}

#include "data_transfer.h"
#include "core/files.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
namespace {
bool sqlite(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && file.read(16) == QByteArray("SQLite format 3\0", 16);
}
bool excluded(const QString &relative, const QString &path) {
    if (relative == "host/instance.lock" || relative == "host/migration.json" ||
        (relative == "host/backups" || relative.startsWith("host/backups/")) ||
        (relative == "host/staging" || relative.startsWith("host/staging/")))
        return true;
    if ((path.endsWith("-wal") || path.endsWith("-shm")) && sqlite(path.left(path.size() - 4)))
        return true;
    return false;
}
QString copySnapshotFiles(const QString &root, const QString &target, QJsonObject *checksums,
                          bool normalizeSqlite = true) {
    if (!QDir().mkpath(target))
        return QStringLiteral("无法创建快照目录");
    QDirIterator iterator(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                          QDirIterator::Subdirectories);
    int count = 0;
    qint64 bytes = 0;
    while (iterator.hasNext()) {
        iterator.next();
        const auto info = iterator.fileInfo();
        const auto relative = QDir(root).relativeFilePath(info.absoluteFilePath());
        if (excluded(relative, info.absoluteFilePath()))
            continue;
        if (Files::reparse(info.absoluteFilePath()))
            return QStringLiteral("数据快照不跟随链接 · ") + relative;
        if (info.isDir())
            continue;
        if (++count > 100000 || (bytes += info.size()) > 20LL * 1024 * 1024 * 1024)
            return QStringLiteral("快照超过 100000 文件或 20 GiB");
        const auto destination = Files::child(target, relative);
        if (destination.isEmpty() || !QDir().mkpath(QFileInfo(destination).absolutePath()))
            return QStringLiteral("快照路径无效");
        if (normalizeSqlite && sqlite(info.absoluteFilePath())) {
            const auto connection = QUuid::createUuid().toString();
            QString error;
            {
                auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
                db.setConnectOptions("QSQLITE_OPEN_READONLY");
                db.setDatabaseName(info.absoluteFilePath());
                if (!db.open())
                    error = db.lastError().text();
                else {
                    QSqlQuery query(db);
                    query.prepare("VACUUM INTO ?");
                    query.addBindValue(destination);
                    if (!query.exec())
                        error = query.lastError().text();
                }
                db.close();
            }
            QSqlDatabase::removeDatabase(connection);
            if (!error.isEmpty())
                return error;
        } else if (!QFile::copy(info.absoluteFilePath(), destination))
            return QStringLiteral("无法复制 · ") + relative;
        (*checksums)[relative] = Files::sha256(destination);
    }
    return {};
}
QString verify(const QString &source) {
    QString error;
    const auto manifest = Files::readObject(source + "/snapshot.json", 32 * 1024 * 1024, &error);
    if (!error.isEmpty() || manifest.value("format") != QJsonValue(1))
        return QStringLiteral("不兼容或损坏的数据快照");
    const auto checksums = manifest.value("files").toObject();
    if (checksums.isEmpty() || checksums.size() > 100000)
        return QStringLiteral("快照文件索引无效");
    for (auto it = checksums.begin(); it != checksums.end(); ++it) {
        const auto file = Files::child(source, it.key(), true);
        if (file.isEmpty() || !QFileInfo(file).isFile() || it.value().toString().size() != 64 ||
            excluded(it.key(), file) || Files::sha256(file) != it.value().toString())
            return QStringLiteral("快照校验失败 · ") + it.key();
    }
    QDirIterator actual(source, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (actual.hasNext()) {
        const auto file = actual.next();
        const auto relative = QDir(source).relativeFilePath(file);
        if (relative != "snapshot.json" && !checksums.contains(relative))
            return QStringLiteral("快照包含未索引文件 · ") + relative;
    }
    const auto dbPath = Files::child(source, "host/state.sqlite", true);
    if (!dbPath.isEmpty()) {
        const auto connection = QUuid::createUuid().toString();
        int version = 0;
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setConnectOptions("QSQLITE_OPEN_READONLY");
            db.setDatabaseName(dbPath);
            if (!db.open())
                error = db.lastError().text();
            else {
                QSqlQuery query(db);
                if (query.exec("PRAGMA user_version") && query.next())
                    version = query.value(0).toInt();
                else
                    error = query.lastError().text();
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
        if (!error.isEmpty() || version > 1)
            return QStringLiteral("快照数据库损坏或版本过新");
    }
    return {};
}
bool rename(const QString &source, const QString &target) {
    QDir().mkpath(QFileInfo(target).absolutePath());
    return QDir().rename(source, target);
}
} // namespace
QString DataTransfer::exportSnapshot(const QString &root, const QString &destination) {
    if (Files::reparse(root))
        return QStringLiteral("数据根不能为链接");
    const auto base = QFileInfo(root).canonicalFilePath();
    const auto target = QDir(destination).absolutePath();
    if (base.isEmpty() || target.compare(base, Qt::CaseInsensitive) == 0 ||
        target.startsWith(base + "/", Qt::CaseInsensitive) || QFileInfo::exists(target))
        return QStringLiteral("备份目标必须为数据根之外的新目录");
    const auto stage = target + ".staging-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject checksums;
    auto error = copySnapshotFiles(base, stage, &checksums);
    if (error.isEmpty() && !Files::writeObject(stage + "/snapshot.json",
                                               {{"format", 1}, {"files", checksums}}, &error)) {
    }
    if (error.isEmpty())
        error = verify(stage);
    if (error.isEmpty() && !QDir().rename(stage, target))
        error = QStringLiteral("快照启用失败");
    if (!error.isEmpty())
        QDir(stage).removeRecursively();
    return error;
}
QString DataTransfer::recover(const QString &root) {
    const auto path = root + "/host/migration.json";
    if (!QFileInfo::exists(path))
        return {};
    QString error;
    const auto journal = Files::readObject(path, 1024 * 1024, &error);
    if (!error.isEmpty())
        return QStringLiteral("迁移记录损坏，保留全部数据");
    if (journal.value("status") == "completed")
        return {};
    const auto moves = journal.value("moves").toArray();
    for (int index = moves.size() - 1; index >= 0; --index) {
        const auto move = moves[index].toObject();
        const auto from = Files::child(root, move.value("from").toString());
        const auto to = Files::child(root, move.value("to").toString());
        if (from.isEmpty() || to.isEmpty())
            return QStringLiteral("迁移记录路径无效");
        if (QFileInfo::exists(to) == QFileInfo::exists(from))
            return QStringLiteral("迁移路径状态有歧义，请保留备份与迁移记录并人工核对");
        if (QFileInfo::exists(to) && !QFileInfo::exists(from) && !rename(to, from))
            return QStringLiteral("迁移恢复失败，请保留 host/backups 与 migration.json");
    }
    QFile::remove(path);
    return {};
}
QString DataTransfer::restoreSnapshot(const QString &root, const QString &source) {
    auto error = verify(source);
    if (!error.isEmpty())
        return error;
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto stage = root + "/host/staging/" + id;
    QJsonObject ignored;
    error = copySnapshotFiles(source, stage, &ignored, false);
    if (error.isEmpty())
        error = verify(stage);
    if (!error.isEmpty()) {
        QDir(stage).removeRecursively();
        return error;
    }
    QFile::remove(stage + "/snapshot.json");
    QJsonArray moves;
    const auto current =
        QDir(root).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const auto &entry : current)
        if (entry.fileName() != "host")
            moves.append(QJsonObject{{"from", entry.fileName()},
                                     {"to", "host/backups/" + id + "/" + entry.fileName()}});
    for (const auto &entry :
         QDir(root + "/host")
             .entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
        if (QStringList{"instance.lock", "migration.json", "backups", "staging"}.contains(
                entry.fileName()))
            continue;
        moves.append(QJsonObject{{"from", "host/" + entry.fileName()},
                                 {"to", "host/backups/" + id + "/host/" + entry.fileName()}});
    }
    for (const auto &entry :
         QDir(stage).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden))
        if (entry.fileName() != "host")
            moves.append(QJsonObject{{"from", "host/staging/" + id + "/" + entry.fileName()},
                                     {"to", entry.fileName()}});
    for (const auto &entry :
         QDir(stage + "/host")
             .entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden))
        if (entry.fileName() != "instance.lock" && entry.fileName() != "migration.json")
            moves.append(QJsonObject{{"from", "host/staging/" + id + "/host/" + entry.fileName()},
                                     {"to", "host/" + entry.fileName()}});
    QJsonObject journal{{"status", "moving"}, {"moves", QJsonArray{}}};
    for (const auto &value : moves) {
        auto recorded = journal.value("moves").toArray();
        recorded.append(value);
        journal["moves"] = recorded;
        if (!Files::writeObject(root + "/host/migration.json", journal, &error))
            break;
        const auto move = value.toObject();
        const auto from = Files::child(root, move.value("from").toString(), true);
        const auto to = Files::child(root, move.value("to").toString());
        if (from.isEmpty() || to.isEmpty() || QFileInfo::exists(to) || !rename(from, to)) {
            error = QStringLiteral("导入移动失败，开始回滚");
            break;
        }
    }
    if (!error.isEmpty()) {
        const auto recovery = recover(root);
        return error + (recovery.isEmpty() ? QString() : QStringLiteral(" · ") + recovery);
    }
    journal["status"] = "completed";
    if (!Files::writeObject(root + "/host/migration.json", journal, &error)) {
        recover(root);
        return error;
    }
    QDir(stage).removeRecursively();
    return {};
}

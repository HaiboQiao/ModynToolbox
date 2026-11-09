#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
namespace Files {
QJsonObject readObject(const QString &path, qint64 maximum, QString *error = nullptr);
bool writeObject(const QString &path, const QJsonObject &value, QString *error = nullptr);
QString child(const QString &root, const QString &relative, bool mustExist = false);
bool reparse(const QString &path);
QStringList resourceKeys(const QStringList &paths);
QString sha256(const QString &path);
bool copyTree(const QString &source, const QString &target, QString *error);
} // namespace Files

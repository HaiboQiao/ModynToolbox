#pragma once
#include <QString>
namespace DataTransfer {
QString exportSnapshot(const QString &root, const QString &destination);
QString restoreSnapshot(const QString &root, const QString &source);
QString recover(const QString &root);
} // namespace DataTransfer

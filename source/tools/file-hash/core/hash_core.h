#pragma once
#include <QCryptographicHash>
#include <QString>
#include <functional>
struct HashResult {
    QString path, digest, error;
    qint64 bytes = 0;
    bool cancelled = false;
};
HashResult hashFile(const QString &path, QCryptographicHash::Algorithm algorithm,
                    const std::function<bool()> &cancelled = {},
                    const std::function<void(qint64, qint64)> &progress = {});

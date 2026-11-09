#include "files.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
QJsonObject Files::readObject(const QString &path, qint64 maximum, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return {};
    }
    const auto bytes = file.read(maximum + 1);
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (bytes.size() > maximum || parse.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("JSON 文件无效或超限");
        return {};
    }
    return document.object();
}
bool Files::writeObject(const QString &path, const QJsonObject &value, QString *error) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error)
            *error = QStringLiteral("目录无法创建");
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}
bool Files::reparse(const QString &path) {
#ifdef Q_OS_WIN
    const auto attributes = GetFileAttributesW(reinterpret_cast<const wchar_t *>(path.utf16()));
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    return QFileInfo(path).isSymLink();
#endif
}
QString Files::child(const QString &root, const QString &relative, bool mustExist) {
    const auto name = QDir::fromNativeSeparators(relative);
    if (name.isEmpty() || QDir::isAbsolutePath(name) || name.contains(':'))
        return {};
    const auto parts = name.split('/');
    for (const auto &part : parts)
        if (part.isEmpty() || part == ".." || part == ".")
            return {};
    const auto base = QFileInfo(root).canonicalFilePath();
    if (base.isEmpty())
        return {};
    QString current = base;
    for (const auto &part : parts) {
        current = QDir(current).absoluteFilePath(part);
        if (reparse(current))
            return {};
    }
    if (mustExist && !QFileInfo::exists(current))
        return {};
    const auto canonical = QFileInfo(current).canonicalFilePath();
    if (!canonical.isEmpty() && !canonical.startsWith(base + "/", Qt::CaseInsensitive))
        return {};
    return QDir::cleanPath(current);
}
QString Files::sha256(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}
bool Files::copyTree(const QString &source, const QString &target, QString *error) {
    if (reparse(source) || !QFileInfo(source).isDir() || !QDir().mkpath(target)) {
        if (error)
            *error = QStringLiteral("无效目录或链接");
        return false;
    }
    qint64 bytes = 0;
    int count = 0;
    QDirIterator iterator(source, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const auto info = iterator.fileInfo();
        const auto relative = QDir(source).relativeFilePath(info.absoluteFilePath());
        const auto origin = child(source, relative, true);
        if (origin.isEmpty() || ++count > 4096 ||
            (bytes += info.isFile() ? info.size() : 0) > 256 * 1024 * 1024) {
            if (error)
                *error = QStringLiteral("目录包含链接或超过包容量限制");
            return false;
        }
        const auto destination = child(target, relative);
        if (destination.isEmpty()) {
            if (error)
                *error = QStringLiteral("目标路径无效");
            return false;
        }
        if (info.isDir()) {
            if (!QDir().mkpath(destination)) {
                if (error)
                    *error = QStringLiteral("无法创建目标目录");
                return false;
            }
        } else {
            if (!QDir().mkpath(QFileInfo(destination).absolutePath()) ||
                !QFile::copy(origin, destination)) {
                if (error)
                    *error = QStringLiteral("复制失败 · ") + relative;
                return false;
            }
        }
    }
    return true;
}

QStringList Files::resourceKeys(const QStringList &paths) {
    QSet<QString> keys;
    for (const auto &path : paths) {
        const QFileInfo file(path);
        auto canonical = file.canonicalFilePath();
        if (canonical.isEmpty()) {
            const auto parent = QDir(file.absolutePath()).canonicalPath();
            canonical = QDir(parent.isEmpty() ? file.absolutePath() : parent)
                            .absoluteFilePath(file.fileName());
        }
        keys.insert("path:" + QDir::cleanPath(canonical).toCaseFolded());
#ifdef Q_OS_WIN
        const auto handle =
            CreateFileW(reinterpret_cast<const wchar_t *>(path.utf16()), FILE_READ_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION info{};
            if (GetFileInformationByHandle(handle, &info))
                keys.insert(QStringLiteral("file:%1:%2:%3")
                                .arg(info.dwVolumeSerialNumber)
                                .arg(info.nFileIndexHigh)
                                .arg(info.nFileIndexLow));
            CloseHandle(handle);
        }
#endif
    }
    return keys.values();
}

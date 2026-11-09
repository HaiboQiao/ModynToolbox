#include "hash_core.h"
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif
HashResult hashFile(const QString &path, QCryptographicHash::Algorithm algorithm,
                    const std::function<bool()> &cancelled,
                    const std::function<void(qint64, qint64)> &progress) {
    HashResult result;
    result.path = QFileInfo(path).absoluteFilePath();
    QFile file(result.path);
    if (!QFileInfo(result.path).isFile()) {
        result.error = QStringLiteral("普通文件不存在");
        return result;
    }
#ifdef Q_OS_WIN
    const auto handle =
        CreateFileW(reinterpret_cast<const wchar_t *>(result.path.utf16()), GENERIC_READ,
                    FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        result.error =
            QStringLiteral("无法取得只读句柄，文件可能被写入或占用 · %1").arg(GetLastError());
        return result;
    }
    const int descriptor =
        _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_RDONLY | _O_BINARY);
    if (descriptor < 0) {
        CloseHandle(handle);
        result.error = QStringLiteral("无法创建文件流");
        return result;
    }
    if (!file.open(descriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        _close(descriptor);
        result.error = file.errorString();
        return result;
    }
#else
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
#endif
    const auto before = QFileInfo(file);
    const auto expectedSize = file.size();
    const auto expectedTime = before.lastModified();
    QCryptographicHash hash(algorithm);
    while (!file.atEnd()) {
        if (cancelled && cancelled()) {
            result.cancelled = true;
            return result;
        }
        const auto block = file.read(1024 * 1024);
        if (file.error() != QFile::NoError || block.isEmpty()) {
            result.error = file.errorString();
            return result;
        }
        hash.addData(block);
        result.bytes += block.size();
        if (progress)
            progress(result.bytes, expectedSize);
    }
    const QFileInfo after(file);
    if (file.size() != expectedSize || after.lastModified() != expectedTime ||
        result.bytes != expectedSize) {
        result.error = QStringLiteral("读取期间文件变化，请重新计算");
        return result;
    }
    result.digest = QString::fromLatin1(hash.result().toHex());
    return result;
}

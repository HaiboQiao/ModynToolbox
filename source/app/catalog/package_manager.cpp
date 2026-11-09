#include "package_manager.h"
#include "core/files.h"
#include "ui/view_spec.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>
PackageManager::PackageManager(QString root) : packagesRoot(QDir(root).absolutePath()) {
    QDir().mkpath(packagesRoot);
}
QString PackageManager::verify(const ToolDefinition &tool) const {
    if (!tool.executable)
        return QStringLiteral("预览包没有执行入口");
    QString error;
    const auto manifest = Files::readObject(tool.packageRoot + "/manifest.json", 65536, &error);
    if (!error.isEmpty())
        return error;
    const auto checksumsPath = Files::child(tool.packageRoot, "checksums.json", true);
    if (checksumsPath.isEmpty() ||
        Files::sha256(checksumsPath) != manifest.value("integrity_sha256").toString())
        return QStringLiteral("工具包摘要索引不一致");
    const auto checksums = Files::readObject(checksumsPath, 1024 * 1024, &error);
    if (!error.isEmpty() || checksums.isEmpty() || checksums.size() > 4096)
        return QStringLiteral("工具包摘要索引无效");
    QDirIterator iterator(tool.packageRoot,
                          QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                          QDirIterator::Subdirectories);
    int count = 0;
    qint64 bytes = 0;
    while (iterator.hasNext()) {
        iterator.next();
        const auto info = iterator.fileInfo();
        if (Files::reparse(info.absoluteFilePath()))
            return QStringLiteral("工具包含链接");
        if (info.isDir())
            continue;
        const auto relative = QDir(tool.packageRoot).relativeFilePath(info.absoluteFilePath());
        if (relative == "manifest.json" || relative == "checksums.json")
            continue;
        if (++count > 4096 || (bytes += info.size()) > 256 * 1024 * 1024)
            return QStringLiteral("工具包超限");
        const auto digest = checksums.value(relative).toString();
        if (digest.size() != 64 || Files::sha256(info.absoluteFilePath()) != digest)
            return QStringLiteral("工具包文件损坏 · ") + relative;
    }
    if (count != checksums.size())
        return QStringLiteral("工具包有缺失文件");
    for (const auto &relative : checksums.keys())
        if (Files::child(tool.packageRoot, relative, true).isEmpty())
            return QStringLiteral("工具包路径越界或缺失");
    return {};
}
QString PackageManager::install(const QString &source) {
    QString error;
    const auto manifest = Files::readObject(source + "/manifest.json", 65536, &error);
    if (!error.isEmpty())
        return error;
    const auto tool = definitionFromManifest(source, manifest, &error);
    if (!error.isEmpty() || !tool.executable)
        return error.isEmpty() ? QStringLiteral("需要正式 v1 工具包") : error;
    error = verify(tool);
    if (!error.isEmpty())
        return error;
    const auto view = loadViewSpec(source, true);
    if (!view.error.isEmpty())
        return view.error;
    const auto destination = packagesRoot + "/" + tool.id + "/" + tool.version;
    if (QFileInfo::exists(destination))
        return QStringLiteral("相同版本已安装，请提高工具版本或先移除闲置版本");
    const auto stage =
        packagesRoot + "/.staging-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!Files::copyTree(source, stage, &error)) {
        QDir(stage).removeRecursively();
        return error;
    }
    auto staged = tool;
    staged.packageRoot = stage;
    error = verify(staged);
    if (!error.isEmpty()) {
        QDir(stage).removeRecursively();
        return error;
    }
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()) ||
        !QDir().rename(stage, destination)) {
        QDir(stage).removeRecursively();
        return QStringLiteral("工具包启用失败，已有版本保留");
    }
    return {};
}
QString PackageManager::remove(const ToolDefinition &tool, bool referenced) {
    if (referenced)
        return QStringLiteral("该版本仍被会话或作业引用，请先结束相关会话");
    const auto path = Files::child(packagesRoot, tool.id + "/" + tool.version, true);
    if (path.isEmpty() || path != QDir(tool.packageRoot).absolutePath())
        return QStringLiteral("工具包归属无效");
    const auto tombstone =
        packagesRoot + "/.removed-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().rename(path, tombstone))
        return QStringLiteral("工具包被占用，移除失败");
    if (!QDir(tombstone).removeRecursively())
        return QStringLiteral("入口已移除，残余目录待释放后清理");
    return {};
}
QVector<ToolDefinition> PackageManager::versions(QStringList *errors) const {
    return readPackages(packagesRoot, errors);
}

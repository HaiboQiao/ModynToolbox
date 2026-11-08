#include "package_catalog.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
CatalogResult readCatalog(const QString &root) {
    CatalogResult result;
    const QRegularExpression identifier("^[a-z][a-z0-9-]{0,63}$");
    const QRegularExpression versionPattern("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$");
    if (root.trimmed().isEmpty()) return result;
    const QDir directory(root);
    for (const auto &toolFolder : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
        QVector<ToolDefinition> versions;
        for (const auto &versionFolder : QDir(toolFolder.absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
            const auto manifestPath = versionFolder.absoluteFilePath() + "/manifest.json";
            QFile manifest(manifestPath);
            if (!manifest.open(QIODevice::ReadOnly) || manifest.size() > 65536) {
                result.errors.append(manifestPath + ": manifest unavailable or too large");
                continue;
            }
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(manifest.readAll(), &error);
            const auto object = document.object();
            const QString id = object.value("tool_id").toString();
            const QString version = object.value("package_version").toString();
            const QString name = object.value("name").toString().trimmed();
            if (error.error != QJsonParseError::NoError || !document.isObject()
                || object.value("manifest_version") != QJsonValue(1)
                || !identifier.match(id).hasMatch() || id != toolFolder.fileName()
                || !versionPattern.match(version).hasMatch() || version != versionFolder.fileName()
                || name.isEmpty() || name.size() > 80
                || (object.contains("description") && !object.value("description").isString())
                || object.value("description").toString().size() > 512) {
                result.errors.append(manifestPath + ": invalid identity, version or schema");
                continue;
            }
            versions.append({id, name, version, object.value("description").toString().left(512), versionFolder.absoluteFilePath()});
        }
        if (!versions.isEmpty()) {
            std::sort(versions.begin(), versions.end(), [](const auto &a, const auto &b) {
                const auto left = a.version.split('.');
                const auto right = b.version.split('.');
                for (int i = 0; i < 3; ++i) {
                    if (left[i].size() != right[i].size()) return left[i].size() > right[i].size();
                    if (left[i] != right[i]) return left[i] > right[i];
                }
                return false;
            });
            result.tools.append(versions.front());
        }
    }
    std::sort(result.tools.begin(), result.tools.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
    return result;
}

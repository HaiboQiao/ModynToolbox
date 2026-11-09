#include "package_catalog.h"
#include "core/files.h"
#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <algorithm>
bool versionGreater(const QString &a, const QString &b) {
    const auto left = a.split('.'), right = b.split('.');
    for (int i = 0; i < 3; ++i) {
        if (left[i].size() != right[i].size())
            return left[i].size() > right[i].size();
        if (left[i] != right[i])
            return left[i] > right[i];
    }
    return false;
}
ToolDefinition definitionFromManifest(const QString &root, const QJsonObject &object,
                                      QString *error) {
    ToolDefinition tool;
    auto invalid = [error](const QString &message) {
        if (error)
            *error = message;
        return ToolDefinition{};
    };
    const QRegularExpression identifier("^[a-z][a-z0-9-]{0,63}$");
    const QRegularExpression version("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$");
    tool.id = object.value("tool_id").toString();
    tool.name = object.value("name").toString().trimmed();
    tool.version = object.value("package_version").toString();
    tool.description = object.value("description").toString();
    tool.category = object.value("category").toString(QStringLiteral("其他"));
    if (tool.category.trimmed().isEmpty() || tool.category.size() > 40 ||
        (object.contains("category") && !object.value("category").isString()))
        return invalid(QStringLiteral("工具分类无效"));
    if (object.contains("keywords") && !object.value("keywords").isArray())
        return invalid(QStringLiteral("工具关键词无效"));
    const auto keywords = object.value("keywords").toArray();
    if (keywords.size() > 20)
        return invalid(QStringLiteral("工具关键词过多"));
    for (const auto &word : keywords) {
        if (!word.isString() || word.toString().size() > 80)
            return invalid(QStringLiteral("工具关键词无效"));
        tool.keywords.append(word.toString());
    }
    tool.icon = object.value("icon").toString("files");
    tool.viewFile = object.value("view_file").toString();
    tool.packageRoot = QDir(root).absolutePath();
    if (object.value("manifest_version") != QJsonValue(1) ||
        !identifier.match(tool.id).hasMatch() || tool.version.size() > 40 ||
        !version.match(tool.version).hasMatch() || tool.name.isEmpty() || tool.name.size() > 80 ||
        tool.description.size() > 512 ||
        (object.contains("description") && !object.value("description").isString()) ||
        (object.contains("icon") && !object.value("icon").isString()) ||
        !QStringList{"image", "code", "files"}.contains(tool.icon) ||
        (object.contains("view_file") && object.value("view_file") != QJsonValue("ui.json")))
        return invalid(QStringLiteral("无效工具标识、版本或元数据"));
    const auto minimum = object.value("min_host_version").toString("1.0.0");
    if (!version.match(minimum).hasMatch() || versionGreater(minimum, QStringLiteral(MODYN_VERSION)))
        return invalid(QStringLiteral("工具需要较新版本的宿主"));
    if (QRegularExpression("^(con|prn|aux|nul|com[1-9]|lpt[1-9])$").match(tool.id).hasMatch())
        return invalid(QStringLiteral("工具标识为 Windows 保留名称"));
    if (!object.contains("protocol_version"))
        return tool;
    if (object.value("protocol_version") != QJsonValue(1) ||
        object.value("ui_schema_version") != QJsonValue(1) || tool.viewFile != "ui.json")
        return invalid(QStringLiteral("不兼容的执行或界面规范"));
    tool.entryProgram = object.value("entry_program").toString();
    if (Files::child(root, tool.entryProgram, true).isEmpty())
        return invalid(QStringLiteral("执行入口缺失或越界"));
    if (!object.value("entry_args").isArray() || object.value("entry_args").toArray().size() > 32)
        return invalid(QStringLiteral("入口参数无效"));
    for (const auto &value : object.value("entry_args").toArray()) {
        if (!value.isString() || value.toString().size() > 1024 ||
            value.toString().contains(QChar(0)))
            return invalid(QStringLiteral("入口参数无效"));
        tool.entryArguments.append(value.toString());
    }
    if (object.value("integrity_file") != QJsonValue("checksums.json") ||
        object.value("integrity_sha256").toString().size() != 64)
        return invalid(QStringLiteral("完整性索引缺失"));
    if (!object.value("limits").isObject())
        return invalid(QStringLiteral("容量声明缺失"));
    tool.limits = object.value("limits").toObject();
    const auto maxFiles = tool.limits.value("max_files").toInt(-1);
    const auto maxBytes = tool.limits.value("max_input_bytes").toInt(-1);
    const auto memory = tool.limits.value("memory_mb").toInt(-1);
    if (maxFiles < 0 || maxFiles > 2000 || maxBytes < 1 || maxBytes > 5 * 1024 * 1024 ||
        memory < 64 || memory > 2048)
        return invalid(QStringLiteral("容量预算无效"));
    const auto actions = object.value("actions").toArray();
    if (actions.isEmpty() || actions.size() > 12)
        return invalid(QStringLiteral("动作数量无效"));
    QSet<QString> identifiers;
    for (const auto &value : actions) {
        if (!value.isObject())
            return invalid(QStringLiteral("动作结构无效"));
        const auto action = value.toObject();
        ActionSpec item;
        item.id = action.value("id").toString();
        item.label = action.value("label").toString();
        item.confirmation = action.value("confirm").toString();
        item.requiredContext = action.value("requires_context").toString();
        for (const auto &key : {"batch", "write_sources", "cancellable", "dynamic_resources"})
            if (action.contains(key) && !action.value(key).isBool())
                return invalid(QStringLiteral("动作策略类型无效"));
        item.batch = action.value("batch").toBool();
        item.dynamicResources = action.value("dynamic_resources").toBool();
        item.writeSources = action.value("write_sources").toBool();
        item.cancellable = action.value("cancellable").toBool(true);
        item.timeoutSeconds = action.value("timeout_seconds").toInt(600);
        if (!identifier.match(item.id).hasMatch() || identifiers.contains(item.id) ||
            item.label.isEmpty() || item.label.size() > 80 || item.confirmation.size() > 1024 ||
            item.requiredContext.size() > 80 || item.timeoutSeconds < 1 ||
            item.timeoutSeconds > 3600)
            return invalid(QStringLiteral("动作标识或执行策略无效"));
        identifiers.insert(item.id);
        tool.actions.append(item);
    }
    tool.executable = true;
    return tool;
}
QVector<ToolDefinition> readPackages(const QString &root, QStringList *errors) {
    QVector<ToolDefinition> tools;
    if (root.trimmed().isEmpty())
        return tools;
    for (const auto &folder :
         QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
        if (folder.fileName().startsWith('.'))
            continue;
        for (const auto &version :
             QDir(folder.absoluteFilePath())
                 .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
            QString error;
            const auto object =
                Files::readObject(version.absoluteFilePath() + "/manifest.json", 65536, &error);
            auto tool = definitionFromManifest(version.absoluteFilePath(), object, &error);
            if (error.isEmpty() &&
                (tool.id != folder.fileName() || tool.version != version.fileName()))
                error = QStringLiteral("工具目录与标识不一致");
            if (!error.isEmpty()) {
                if (errors)
                    errors->append(version.absoluteFilePath() + " · " + error);
                continue;
            }
            tools.append(tool);
        }
    }
    return tools;
}
CatalogResult readCatalog(const QString &root) {
    CatalogResult result;
    const auto tools = readPackages(root, &result.errors);
    QMap<QString, ToolDefinition> selected;
    for (const auto &tool : tools)
        if (!selected.contains(tool.id) ||
            versionGreater(tool.version, selected.value(tool.id).version))
            selected[tool.id] = tool;
    for (const auto &tool : selected)
        result.tools.append(tool);
    return result;
}
